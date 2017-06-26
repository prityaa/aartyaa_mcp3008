
/**
mcp3008 ADC to spi converter driver code
*/

#define DEBUG
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
#include <linux/anon_inodes.h>

#define MCP3008	0

struct mcp3008 {
	struct spi_device	*spi;
	struct spi_message 	msg;
	struct spi_transfer 	transfer[2];
	struct mutex 		lock;
	u8 			tx_buf ____cacheline_aligned;
        u8 			rx_buf[2];
};

static int mcp3008_channel_to_tx_data(void)
{
        int start_bit = 1;
	const unsigned int channel = 0;
	bool differential = 0;

	return ((start_bit << 6) | (!differential << 5) |
                               (channel << 2));
}

static int mcp3008_conversion(struct mcp3008 *mcp)
{
        int ret;

        mcp->rx_buf[0] = 0;
        mcp->rx_buf[1] = 0;
        mcp->tx_buf = mcp3008_channel_to_tx_data();

	ret = spi_sync(mcp->spi, &mcp->msg);
	dev_dbg(&mcp->spi->dev, "mcp3008_conversion : ret = %d\n", ret);
	if (ret < 0)
        	return ret;
	
	dev_dbg(&mcp->spi->dev, "mcp3008_conversion :  rx_buf[0] %x, rx_buf[1] = %x\n", 
		mcp->rx_buf[0], mcp->rx_buf[1]);
	return (mcp->rx_buf[0] << 2 | mcp->rx_buf[1] >> 6);
}


static int mcp3008_aartyaa_show_data(struct device *dev,
                        struct device_attribute *attr,
                        char *buf)
{
        struct spi_device *spi_dev = to_spi_device(dev);
        struct mcp3008 *mcp = spi_get_drvdata(spi_dev);
	int ret = -EINVAL;
	
	if (spi_dev == NULL) {
		dev_err(dev, "mcp3008_aartyaa_show_data : spi_dev is null\n");	
		ret = -1;	
		goto dev_error;
	}

	if (mcp == NULL) {
		dev_err(dev, "mcp3008_aartyaa_show_data : mcp is null\n");	
		ret = -2;	
		goto dev_error;
	}

	dev_dbg(dev, "mcp3008_aartyaa_show_data\n");

        mutex_lock(&mcp->lock);

	ret = mcp3008_conversion(mcp) & 0xff;
	dev_dbg(dev, "mcp3008_aartyaa_show_data : ret = %x\n", ret);
        if (ret < 0)
       		goto out;
out:
	mutex_unlock(&mcp->lock);

dev_error:
	memset(buf, '\0', sizeof(buf));
        return sprintf(buf, "%d\n", ret);
}

struct device_attribute mcp3008_attr_raw_data = {
        .attr = {
                .name = "aartyaa_mcp3008",
                .mode = VERIFY_OCTAL_PERMISSIONS(0664),
        },
        .show   = mcp3008_aartyaa_show_data,
        .store  = NULL,
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
        if (!ptr)
                return NULL;

	dev_dbg(dev, "mcp3008_device_alloc : devres is allocated\n");
        mcp = kzalloc(sizeof(*mcp), GFP_KERNEL);
        if (mcp) {
                *ptr = mcp;
                devres_add(dev, ptr);
		dev_dbg(dev, "mcp3008_device_alloc : dev is added to devres\n");
        } else {
                devres_free(ptr);
		dev_dbg(dev, "mcp3008_device_alloc : falied to allocate memory\n");
        }
        
        return mcp;
}

static int mcp3008_probe(struct spi_device *spi)
{
	int ret = 0;
	struct mcp3008 *mcp = NULL;

	if (spi == NULL) {
		pr_debug("mcp3008_probe : spi is null\n");
		return -EINVAL;
	}

	dev_dbg(&spi->dev, "aaartyaa came in probe, master dev = %s\n",
			 dev_name(&spi->master->dev));

	//mcp = kmalloc(sizeof(struct mcp3008), GFP_KERNEL);
        //mcp = kzalloc(sizeof(*mcp), GFP_KERNEL);
	mcp = mcp3008_device_alloc(&spi->dev, sizeof(*mcp));
	if (!mcp) {
		dev_dbg(&spi->dev, "mcp3008_probe : falied to alloc memory\n");
		return -ENOMEM;
	}

	//dev_dbg(&spi->dev, "mcp3008_probe : kmalloc success\n");
	
	//mcp->spi->dev = spi->dev;
	//mcp->spi->dev.parent = &spi->dev;
	//mcp->spi->dev.of_node = spi->dev.of_node;
        mcp->spi = spi;
	mcp->spi->dev.driver_data = mcp;	

	//dev_dbg(&spi->dev, "mcp3008_probe : assigned spi data to local data\n");
	mcp->transfer[0].tx_buf = &mcp->tx_buf;
        mcp->transfer[0].len = sizeof(mcp->tx_buf);
        mcp->transfer[1].rx_buf = mcp->rx_buf;
        mcp->transfer[1].len = sizeof(mcp->rx_buf);
	
	dev_dbg(&spi->dev, "mcp3008_probe : trasefer buffer is ready\n");
	spi->dev.driver_data = (struct mcp3008 *)mcp;

	//spi_set_drvdata(mcp->spi, mcp);
	
	dev_dbg(&spi->dev, "mcp3008_probe : initing spi msg\n");
	spi_message_init_with_transfers(&mcp->msg, mcp->transfer,
                                        ARRAY_SIZE(mcp->transfer));
	mutex_init(&mcp->lock);
	dev_dbg(&spi->dev, "mcp3008_probe : creating sysfs for mcp3008\n");
	
	if (sysfs_create_group(&mcp->spi->dev.kobj, &mcp3008_attr_grp)) {
 		kobject_put(&spi->dev.kobj);	
		dev_err(&spi->dev, "failed to create sysfs for mcp3008\n");
		kfree(mcp);
		ret = -1;
	}
	
	return ret;
}

static const struct of_device_id mcp3008_of_ids[] = {
	{
		.compatible = "aartyaa_mcp3008",
		.data = (void *) MCP3008,
	},
	
	{ },
};

MODULE_DEVICE_TABLE(of, mcp3008_of_ids);

static const struct spi_device_id mcp3008_ids[] = {
	{"aartyaa_mcp3008", MCP3008},
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
