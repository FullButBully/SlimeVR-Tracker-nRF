#include <zephyr/kernel.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/atomic.h>

static const struct spi_dt_spec imu = SPI_DT_SPEC_GET(DT_NODELABEL(imu_spi),
	SPI_WORD_SET(8) | SPI_MODE_CPOL | SPI_MODE_CPHA, 2);
static const struct gpio_dt_spec irq = GPIO_DT_SPEC_GET(DT_PATH(zephyr_user), int0_gpios);
static struct gpio_callback cb;
static atomic_t edges;

static void interrupt(const struct device *dev, struct gpio_callback *callback, uint32_t pins)
{
	ARG_UNUSED(dev); ARG_UNUSED(callback); ARG_UNUSED(pins);
	atomic_inc(&edges);
}

static int read_reg(uint8_t reg, uint8_t *data, size_t len)
{
	uint8_t addr = reg | 0x80;
	struct spi_buf txb = {.buf = &addr, .len = 1};
	struct spi_buf rxb[] = {{.buf = NULL, .len = 1}, {.buf = data, .len = len}};
	struct spi_buf_set tx = {.buffers = &txb, .count = 1};
	struct spi_buf_set rx = {.buffers = rxb, .count = 2};
	return spi_transceive_dt(&imu, &tx, &rx);
}

static int write_reg(uint8_t reg, uint8_t value)
{
	uint8_t bytes[] = {reg, value};
	struct spi_buf buf = {.buf = bytes, .len = sizeof(bytes)};
	struct spi_buf_set tx = {.buffers = &buf, .count = 1};
	return spi_write_dt(&imu, &tx);
}

int main(void)
{
	k_msleep(100);
	printk("\nLSM6DSV validation: bus=%s mode=3 requested=%uHz INT1=%s.%u\n",
		imu.bus->name, imu.config.frequency, irq.port->name, irq.pin);
	printk("External memory disconnected; pin mapping is defined by selected overlays\n");
	if (!spi_is_ready_dt(&imu) || !gpio_is_ready_dt(&irq)) {
		printk("FAIL: SPI/GPIO device not ready\n");
		return 0;
	}
	int err = gpio_pin_configure_dt(&irq, GPIO_INPUT | GPIO_PULL_UP);
	gpio_init_callback(&cb, interrupt, BIT(irq.pin));
	err |= gpio_add_callback(irq.port, &cb);
	err |= gpio_pin_interrupt_configure_dt(&irq, GPIO_INT_EDGE_FALLING);
	printk("INT1 setup=%d raw=%d\n", err, gpio_pin_get_raw(irq.port, irq.pin));
	unsigned good = 0;
	for (int i = 0; i < 32; i++) {
		uint8_t id = 0;
		err = read_reg(0x0f, &id, 1);
		good += !err && id == 0x70;
		printk("WHO_AM_I[%d]=0x%02x err=%d\n", i, id, err);
		k_msleep(10);
	}
	printk("IDENTITY: %u/32 expected 0x70\n", good);
	if (good != 32) {
		printk("FAIL: inspect power, ground, CS and SDI/SDO continuity; sampling skipped\n");
		return 0;
	}
	err = write_reg(0x12, 0x01);
	uint8_t ctrl3 = 1;
	int tries;
	for (tries = 0; tries < 100 && !err && (ctrl3 & 1); tries++) {
		k_msleep(1);
		err = read_reg(0x12, &ctrl3, 1);
	}
	if (err || (ctrl3 & 1)) {
		printk("FAIL: reset err=%d ctrl3=0x%02x\n", err, ctrl3);
		return 0;
	}
	/* BDU + auto-increment; +/-2g, +/-250dps; 120Hz high performance.
	 * Active-low open-drain INT1, accelerometer data-ready routed to INT1.
	 */
	const uint8_t regs[][2] = {{0x12, 0x44}, {0x15, 0x01}, {0x17, 0x00},
		{0x03, 0x18}, {0x10, 0x06}, {0x11, 0x06}, {0x0d, 0x01}};
	for (size_t i = 0; i < ARRAY_SIZE(regs); i++) {
		uint8_t actual = 0;
		err = write_reg(regs[i][0], regs[i][1]);
		err |= read_reg(regs[i][0], &actual, 1);
		printk("REG 0x%02x wrote=0x%02x read=0x%02x err=%d\n",
			regs[i][0], regs[i][1], actual, err);
		if (err || actual != regs[i][1]) {
			printk("FAIL: register readback\n");
			return 0;
		}
	}
	unsigned samples = 0, errors = 0, int_low = 0, int_high = 0;
	int64_t report = k_uptime_get();
	int16_t accel[3] = {0}, gyro[3] = {0};
	for (;;) {
		uint8_t status = 0, data[12];
		err = read_reg(0x1e, &status, 1);
		int level = gpio_pin_get_raw(irq.port, irq.pin);
		int_low += level == 0;
		int_high += level == 1;
		if (!err && (status & 3) == 3) {
			err = read_reg(0x22, data, sizeof(data));
			if (!err) {
				for (int i = 0; i < 3; i++) {
					gyro[i] = (int16_t)sys_get_le16(data + i * 2);
					accel[i] = (int16_t)sys_get_le16(data + 6 + i * 2);
				}
				samples++;
			}
		}
		errors += err != 0;
		if (k_uptime_get() - report >= 1000) {
			printk("t=%lld samples=%u errors=%u irq=%ld pin=%d status=0x%02x a_mg=[%d,%d,%d] g_mdps=[%d,%d,%d] int_low=%u int_high=%u\n",
				k_uptime_get(), samples, errors, (long)atomic_get(&edges),
				gpio_pin_get_raw(irq.port, irq.pin), status,
				accel[0]*61/1000, accel[1]*61/1000, accel[2]*61/1000,
				gyro[0]*875/100, gyro[1]*875/100, gyro[2]*875/100, int_low, int_high);
			report = k_uptime_get();
		}
		k_msleep(1);
	}
}
