
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/of.h>



#define IRIS2P_I2C_NAME		"iris2p_i2c"
#define IRIS2P_I2C_ADDR		0x22

static struct mutex i2c_lock;


static const struct i2c_device_id iris_i2c_id[] = {
	{ IRIS2P_I2C_NAME, 0 },{ }
};
static struct of_device_id iris_i2c_match_table[] = {
	{.compatible = "pxlw,iris2p_i2c"},
	{ },
};

static struct i2c_client *iris_i2c_client = NULL;


int iris_i2c_reg_write(unsigned int addr, unsigned int data)
{
	struct i2c_msg msg;
	int ret = -1;
	unsigned char buf[9];

	if(!iris_i2c_client)
		return ret;

	mutex_lock(&i2c_lock);
	buf[0] = 0xcc;	//type
	buf[1] = addr & 0xff;
	buf[2] = (addr >> 8) & 0xff;
	buf[3] = (addr >> 16) & 0xff;
	buf[4] = (addr >> 24) & 0xff;
	buf[5] = (data >> 24) & 0xff;
	buf[6] = (data >> 16) & 0xff;
	buf[7] = (data >> 8) & 0xff;
	buf[8] = data & 0xff;

	msg.flags = !I2C_M_RD;
	msg.addr  = IRIS2P_I2C_ADDR;
	msg.len   = 9;
	msg.buf   = buf;

	ret = i2c_transfer(iris_i2c_client->adapter, &msg, 1);
	if (ret < 1)
		pr_err("%s: I2C WRITE FAILED=[%d]\n", __func__, ret);

	mutex_unlock(&i2c_lock);

	return ret;
}

int iris_i2c_reg_read(unsigned int addr, unsigned int *data)
{
	struct i2c_msg msgs[2];
	int ret = -1;
	unsigned char wr_buf[5];
	unsigned char rd_buf[4];

	if(!iris_i2c_client)
		return ret;

	mutex_lock(&i2c_lock);
	wr_buf[0] = 0xcc;	//type
	wr_buf[1] = addr & 0xff;
	wr_buf[2] = (addr >> 8) & 0xff;
	wr_buf[3] = (addr >> 16) & 0xff;
	wr_buf[4] = (addr >> 24) & 0xff;

	msgs[0].flags = !I2C_M_RD;
	msgs[0].addr  = IRIS2P_I2C_ADDR;
	msgs[0].len   = 5;
	msgs[0].buf   = wr_buf;

	msgs[1].flags = I2C_M_RD;
	msgs[1].addr  = IRIS2P_I2C_ADDR;
	msgs[1].len   = 4;
	msgs[1].buf   = rd_buf;

	ret = i2c_transfer(iris_i2c_client->adapter, msgs, 2);
	if (ret < 1)
		pr_err("%s: I2C READ FAILED=[%d]\n", __func__, ret);

	*data = (rd_buf[0] << 24) + (rd_buf[1] << 16) + (rd_buf[2] << 8) + rd_buf[3];
	mutex_unlock(&i2c_lock);

	return ret;
}


static int iris_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{

	if(NULL == client->dev.of_node)
	{
        return -EINVAL;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("iris2p i2c_check_functionality failed\n");
		return -ENODEV;
	}

	iris_i2c_client = client;
	return 0;
}

static int iris_i2c_remove(struct i2c_client *client)
{
	iris_i2c_client = NULL;
	return 0;
}

static struct i2c_driver iris_i2c_driver = {
	.probe		= iris_i2c_probe,
	.remove		= iris_i2c_remove,
	.id_table	= iris_i2c_id,
	.driver	= {
		.name	= IRIS2P_I2C_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = iris_i2c_match_table,
	},
};

static int __init iris_i2c_init(void)
{
	int ret;
	pr_debug("Enter\n");

	ret = i2c_add_driver(&iris_i2c_driver);
	if (ret) {
		pr_err("Unable to register driver (%d)\n", ret);
		return ret;
	}
	mutex_init(&i2c_lock);

	pr_debug("Exit\n");

	return ret;
}


static void __exit iris_i2c_exit(void)
{
	pr_debug("Enter\n");
	i2c_del_driver(&iris_i2c_driver);
	pr_debug("Exit\n");
}


module_init(iris_i2c_init);
module_exit(iris_i2c_exit);

MODULE_AUTHOR("<pixelworks>");
MODULE_DESCRIPTION("Iris2p I2C Driver");
MODULE_LICENSE("GPL v2");
