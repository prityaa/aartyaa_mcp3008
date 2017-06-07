
/**
mcp3008 ADC to spi converter driver code
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/spi/spi.h>
#include <linux/of_device.h>
#include <linux/mod_devicetable.h>
#include <linux/iio/iio.h>
#include <linux/regulator/consumer.h>


#define MCP3008	0
#define MCP3004	1

#define MCP320X_VOLTAGE_CHANNEL_DIFF(chan1, chan2)              \
        {                                                       \
                .type = IIO_VOLTAGE,                            \
                .indexed = 1,                                   \
                .channel = (chan1),                             \
                .channel2 = (chan2),                            \
                .address = (chan1),                             \
                .differential = 1,                              \
                .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),   \
                .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) \
        }

#define MCP320X_VOLTAGE_CHANNEL(num)                            \
        {                                                       \
                .type = IIO_VOLTAGE,                            \
                .indexed = 1,                                   \
                .channel = (num),                               \
                .address = (num),                               \
                .info_mask_separate = BIT(IIO_CHAN_INFO_RAW),   \
                .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE) \
        }

struct mcp3008_chip_info {
        const struct iio_chan_spec *channels;
        unsigned int num_channels;
        unsigned int resolution;
};

struct mcp3008 {
	struct spi_device *spi;
	struct spi_message msg;
	struct spi_transfer transfer[2];
	const struct mcp3008_chip_info *chip_info;
	struct regulator *reg;	
	struct mutex lock;
	u8 tx_buf ____cacheline_aligned;
        u8 rx_buf[2];
};

static const struct iio_chan_spec mcp3201_channels[] = {
        MCP320X_VOLTAGE_CHANNEL_DIFF(0, 1),
};


static const struct iio_chan_spec mcp3008_channels[] = {
        MCP320X_VOLTAGE_CHANNEL(0),
        MCP320X_VOLTAGE_CHANNEL(1),
        MCP320X_VOLTAGE_CHANNEL(2),
        MCP320X_VOLTAGE_CHANNEL(3),
        MCP320X_VOLTAGE_CHANNEL(4),
        MCP320X_VOLTAGE_CHANNEL(5),
        MCP320X_VOLTAGE_CHANNEL(6),
        MCP320X_VOLTAGE_CHANNEL(7),
        MCP320X_VOLTAGE_CHANNEL_DIFF(0, 1),
        MCP320X_VOLTAGE_CHANNEL_DIFF(1, 0),
        MCP320X_VOLTAGE_CHANNEL_DIFF(2, 3),
        MCP320X_VOLTAGE_CHANNEL_DIFF(3, 2),
        MCP320X_VOLTAGE_CHANNEL_DIFF(4, 5),
        MCP320X_VOLTAGE_CHANNEL_DIFF(5, 4),
        MCP320X_VOLTAGE_CHANNEL_DIFF(6, 7),
        MCP320X_VOLTAGE_CHANNEL_DIFF(7, 6),
};
static struct mcp3008_chip_info mcp3008_chip_info = {
	.channels = mcp3008_channels,
	.num_channels = ARRAY_SIZE(mcp3008_channels),
	.resolution = 10
};

static struct attribute *mcp3008_attr[] = {
	&mcp3008_attr_raw_data.attr,
	NULL,
};

static struct attribute_group mcp3008_attr_grp = {
	.attrs = mcp3008_attr,
};

static void mcp3008_device_free(struct mcp3008 *dev)
{
        if (dev) {
                put_device(&dev->spi->dev);
		kfree(dev);
	}
}

static void devm_mcp3008_device_release(struct device *dev, void *res)
{
	dev_dbg(dev, "mcp3008_device_release : releasing the mcp3008 pointer\n");
        mcp3008_device_free(*(struct mcp3008 **)res);
}

struct mcp3008 *mcp3008_device_alloc(struct device *dev, int sizeof_priv)
{
        struct mcp3008 **ptr, *mcp;
               
        ptr = devres_alloc(devm_mcp3008_device_release, sizeof(*ptr),GFP_KERNEL); 
	dev_dbg(dev, "mcp3008_device_alloc : allocation memory for mcp3008\n");
        if (!ptr)
                return NULL;

        mcp = kzmalloc(sizeof_priv, GFP_KERNEL);
        if (mcp) {
                *ptr = mcp;
                devres_add(dev, ptr);
        } else {
                devres_free(ptr);
        }
        
        return mcp;
}

static int mcp3008_probe(struct spi_device *spi)
{
	int ret = 0;
	struct mcp3008 *mcp;
	const struct mcp3008_chip_info *chip_info;

	pr_debug("mcp3008_probe : aartyaa came in probe");
	dev_dbg(&spi->dev, "aaartyaa came in probe\n");

	//mcp = kmalloc(sizeof(mcp), GFP_KERNEL);
	mcp = mcp3008_device_alloc(&spi->dev, GFP_KERNEL);
	if (!mcp) {
		pr_debug("mcp3008_probe : falied to alloc memory\n");
		dev_dbg(&spi->dev, "mcp3008_probe : falied to alloc memory\n");
		return -ENOMEM;
	}

        mcp->spi = spi;
        chip_info = &mcp3008_chip_info;
        mcp->chip_info = chip_info;

        mcp->transfer[0].tx_buf = &mcp->tx_buf;
        mcp->transfer[0].len = sizeof(mcp->tx_buf);
        mcp->transfer[1].rx_buf = mcp->rx_buf;
        mcp->transfer[1].len = sizeof(mcp->rx_buf);

        spi_message_init_with_transfers(&mcp->msg, mcp->transfer,
                                        ARRAY_SIZE(mcp->transfer));

	mcp->reg = devm_regulator_get(&spi->dev, "vref");
        if (IS_ERR(mcp->reg))
                return PTR_ERR(mcp->reg);

        ret = regulator_enable(mcp->reg);
        if (ret < 0)
                return ret;

	mutex_init(&mcp->lock);
	if (sysfs_create_group(&spi->dev.kobj, &mcp3008_attr_grp)) {
 		kobject_put(&spi->dev.kobj);	
		ret = -1;
	}
	
	return ret;
}

static const struct of_device_id mcp3008_of_ids[] = {
	{
		.compatible = "aartyaa_mcp3008",
		.data = (void *) MCP3008,
	},
	
	{
		.compatible = "aartyaa_mcp3004",
		.data = (void *) MCP3004,
	},

	{ },
};

MODULE_DEVICE_TABLE(of, mcp3008_of_ids);

static const struct spi_device_id mcp3008_ids[] = {
	{"aartyaa_mcp3008", MCP3008},
	{"aartyaa_mcp3004", MCP3004},
	{},
};

MODULE_DEVICE_TABLE(spi, mcp3008_ids);

static struct spi_driver mcp3008_driver = {
	.driver = {
		.name = "aartyaa_mcp3008",
		.of_match_table = of_match_ptr(mcp3008_of_ids),
	},
	.id_table = mcp3008_ids,
	.probe = mcp3008_probe,
	//.remove = devm_mcp3008_device_release,
};

module_spi_driver(mcp3008_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("prityaa");
MODULE_DESCRIPTION("AARTYAA MCP3008 driver code");
