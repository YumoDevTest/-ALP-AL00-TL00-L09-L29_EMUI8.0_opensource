/*
 * asp_hdmi_dma.c -- asp dma driver
 *
 * Copyright (c) 2017 Hisilicon Technologies CO., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/hwspinlock.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include "asp_hdmi_dma.h"

/*lint -e774 -e747 -e502 -e429*/

/* channel info:channel_num, reg_value */
const unsigned int asp_hdmi_channel_num[CHANNEL_NUM_MAX][2] = {
	[CHANNEL_NUM_1] = {1, 0x0},
	[CHANNEL_NUM_2] = {2, 0x1},
	[CHANNEL_NUM_4] = {4, 0x3},
	[CHANNEL_NUM_6] = {6, 0x5},
	[CHANNEL_NUM_8] = {8, 0x7}
};

/* bit_width info:bit_width, reg_value */
const unsigned int asp_hdmi_bit_width[BIT_WIDTH_MAX][2] = {
	[BIT_WIDTH_16] = {16, 0x0},
	[BIT_WIDTH_17] = {17, 0x1},
	[BIT_WIDTH_18] = {18, 0x2},
	[BIT_WIDTH_19] = {19, 0x3},
	[BIT_WIDTH_20] = {20, 0x4},
	[BIT_WIDTH_21] = {21, 0x5},
	[BIT_WIDTH_22] = {22, 0x6},
	[BIT_WIDTH_23] = {23, 0x7},
	[BIT_WIDTH_24] = {24, 0x8}
};

/* align type:align type, reg_value */
const unsigned int asp_hdmi_align_type[ALIGN_MAX][2] = {
	[ALIGN_16] = {16, 0x1},
	[ALIGN_32] = {32, 0x0}
};

/* sample rate info: sample rate,reg_value */
const unsigned int asp_hdmi_sio_sample_rate[SAMPLE_RATE_MAX][2] = {
	[SAMPLE_RATE_44] = {44, 0x00},
	[SAMPLE_RATE_48] = {48, 0x02},
	[SAMPLE_RATE_32] = {32, 0x03},
	[SAMPLE_RATE_96] = {96, 0x0A}
};

struct asp_hdmi_dma_priv {
	struct device *dev;
	spinlock_t lock;
	struct resource *res;
	struct wake_lock wake_lock;
	void __iomem *asp_dma_reg_base_addr;
};

static struct asp_hdmi_dma_priv *asp_hdmi_dma_priv = NULL;

static unsigned int asp_hdmi_dmac_reg_read(unsigned int reg)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;
	unsigned int ret = 0;
	unsigned long flag_sft = 0;

	BUG_ON(NULL == priv);

	spin_lock_irqsave(&priv->lock, flag_sft);
	ret = readl(priv->asp_dma_reg_base_addr + reg);
	spin_unlock_irqrestore(&priv->lock, flag_sft);

	return ret;
}

static void asp_hdmi_dmac_reg_write(unsigned int reg, unsigned int value)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;
	unsigned long flag_sft = 0;

	BUG_ON(NULL == priv);

	spin_lock_irqsave(&priv->lock, flag_sft);
	writel(value, priv->asp_dma_reg_base_addr + reg);
	spin_unlock_irqrestore(&priv->lock, flag_sft);
}

static void asp_hdmi_dmac_reg_set_bit(unsigned int reg, unsigned int offset)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;
	unsigned int value = 0;
	unsigned long flag_sft = 0;

	BUG_ON(NULL == priv);

	spin_lock_irqsave(&priv->lock, flag_sft);

	value = readl(priv->asp_dma_reg_base_addr + reg);
	value |= (1 << offset);
	writel(value, priv->asp_dma_reg_base_addr + reg);

	spin_unlock_irqrestore(&priv->lock, flag_sft);
}

static void asp_hdmi_dmac_reg_clr_bit(unsigned int reg, unsigned int offset)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;
	unsigned int value = 0;
	unsigned long flag_sft = 0;

	BUG_ON(NULL == priv);

	spin_lock_irqsave(&priv->lock, flag_sft);

	value = readl(priv->asp_dma_reg_base_addr + reg);
	value &= ~(1 << offset);
	writel(value, priv->asp_dma_reg_base_addr + reg);

	spin_unlock_irqrestore(&priv->lock, flag_sft);
}

#ifdef ASP_HDMI_DMA_DEBUG
static void asp_hdmi_dmac_dump(void)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;

	BUG_ON(NULL == priv);

	dev_dbg(priv->dev, "REG_HDMI_TX3:0x%x\n", asp_hdmi_dmac_reg_read(ASP_HDMI_TX3));
	dev_dbg(priv->dev, "REG_ASP_HDMI_DMA_EN:0x%x\n", asp_hdmi_dmac_reg_read(ASP_HDMI_DMA_EN));
	dev_dbg(priv->dev, "REG_ASP_HDMI_INT_EN:0x%x\n", asp_hdmi_dmac_reg_read(ASP_HDMI_INT_EN));
	dev_dbg(priv->dev, "REG_A_ADD:0x%pK\n", (void *)(unsigned long)asp_hdmi_dmac_reg_read(ASP_HDMI_A_ADDR));
	dev_dbg(priv->dev, "REG_A_LEN:0x%x\n", asp_hdmi_dmac_reg_read(ASP_HDMI_A_LEN));
	dev_dbg(priv->dev, "REG_B_ADD:0x%pK\n", (void *)(unsigned long)asp_hdmi_dmac_reg_read(ASP_HDMI_B_ADDR));
	dev_dbg(priv->dev, "REG_B_LEN:0x%x\n", asp_hdmi_dmac_reg_read(ASP_HDMI_B_LEN));
}
#endif

static int asp_hdmi_transform_to_tx3_reg_value(struct tx3_config_parameters parameters, unsigned int *reg_value)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;
	unsigned int value = 0x0;

	BUG_ON(NULL == priv);

	if (BIT_WIDTH_MAX - 1 < parameters.bit_width) {
		dev_err(priv->dev, "[%s:%d]  not support parame:bit_width =%d \n", __func__, __LINE__, parameters.bit_width);
		return -1;
	}

	if ((CHANNEL_NUM_MAX - 1 < parameters.channel_num)
		|| (CHANNEL_NUM_1 == parameters.channel_num && BIT_WIDTH_16 != parameters.bit_width)) {
		dev_err(priv->dev, "[%s:%d]  not support parame:channel= %d and bit_width =%d\n", __func__, __LINE__, parameters.channel_num, parameters.bit_width);
		return -1;
	}

	if (ALIGN_16 != parameters.align_type && ALIGN_32 != parameters.align_type) {
		dev_err(priv->dev, "[%s:%d]  not support parame:align_type= %d\n", __func__, __LINE__, parameters.align_type);
		return -1;
	}

	value = (asp_hdmi_bit_width[parameters.bit_width][1] << 3)
			| (HDMI_PCM_SWITCH_ORDE_MAST << HDMI_PCM_SWITCH_ORDE_BIT)
			| (asp_hdmi_channel_num[parameters.channel_num][1] << 8)
			| (asp_hdmi_align_type[parameters.align_type][1] << 7)
			| HDMI_TX3_EN_MASK;
	*reg_value = value;

#ifdef ASP_HDMI_DMA_DEBUG
	dev_dbg(priv->dev, "parameters.bit_width:0x%x\n", parameters.bit_width);
	dev_dbg(priv->dev, "parameters.channel_num:0x%x\n", parameters.channel_num);
	dev_dbg(priv->dev, "parameters.align_type:0x%x\n", parameters.align_type);
	dev_dbg(priv->dev, "value:0x%x\n", value);
#endif

	return 0;

}


void asp_hdmi_dma_enable(void)
{
	asp_hdmi_dmac_reg_write(ASP_HDMI_DMA_EN, HDMI_DMA_EN_MASK);
}

unsigned int asp_hdmi_reg_read_irsr(void)
{
	return asp_hdmi_dmac_reg_read(ASP_HDMI_INT_STATE);
}

int asp_hdmi_tx3_config(struct tx3_config_parameters parameters)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;
	unsigned int reg_value = 0x0;
	int ret = 0;

	BUG_ON(NULL == priv);

	ret = asp_hdmi_transform_to_tx3_reg_value(parameters, &reg_value);

	if (0 != ret) {
		dev_err(priv->dev, "[%s:%d]  asp_hdmi_tx3_config fail.\n", __func__, __LINE__);
		return -1;
	}

	asp_hdmi_dmac_reg_write(ASP_HDMI_TX3, reg_value);

#ifdef ASP_HDMI_DMA_DEBUG
	asp_hdmi_dmac_dump();
#endif

	dev_info(priv->dev, "asp_hdmi_tx3_config succ.\n");

	return 0;
}

void asp_hdmi_tx3_enable(void)
{
	asp_hdmi_dmac_reg_set_bit(ASP_HDMI_TX3, HDMI_TX3_EN_BIT);
	pr_info("[%s:%d],asp_hdmi_tx3_enable.\n", __FUNCTION__, __LINE__);
}

void asp_hdmi_tx3_disable(void)
{
	if (1 == (asp_hdmi_dmac_reg_read(ASP_HDMI_TX3) & 0x1)){
		asp_hdmi_dmac_reg_clr_bit(ASP_HDMI_TX3, HDMI_TX3_EN_BIT);
		pr_info("[%s:%d],asp_hdmi_tx3_disable.\n", __FUNCTION__, __LINE__);
	}
	else{
		pr_info("[%s:%d],asp_hdmi_tx3_disable do nothing.\n", __FUNCTION__, __LINE__);
	}

}

void asp_hdmi_SIO_config(struct sio_config_parameters parameters)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;
	unsigned int reg_value = 0x0;
	//int ret = 0;

	BUG_ON(NULL == priv);

	/* set channel type */
	asp_hdmi_dmac_reg_set_bit(ASP_HDMI_SIO_CH0_STATUS1_L, HDMI_SIO_CHANNEL_TYPE_BIT);
	asp_hdmi_dmac_reg_set_bit(ASP_HDMI_SIO_CH0_STATUS1_R, HDMI_SIO_CHANNEL_TYPE_BIT);

	/* set sample rate */
	if (parameters.sample_rate < SAMPLE_RATE_NO_SUPPORT){
		reg_value = asp_hdmi_dmac_reg_read(ASP_HDMI_SIO_CH0_STATUS1_L);
		reg_value |=  (asp_hdmi_sio_sample_rate[parameters.sample_rate][1]) << HDMI_SIO_SAMPLE_RATE_BIT;
		asp_hdmi_dmac_reg_write(ASP_HDMI_SIO_CH0_STATUS1_L, reg_value);
		asp_hdmi_dmac_reg_write(ASP_HDMI_SIO_CH0_STATUS1_R, reg_value);
		dev_info(priv->dev, "[%s:%d],set sio sample rate(%d).\n", __FUNCTION__, __LINE__, parameters.sample_rate);
	}
	else{
		dev_err(priv->dev, "[%s:%d],sample rate(%d) is no support,and will use defaulte sio config.\n", __FUNCTION__, __LINE__, parameters.sample_rate);
	}

	/* set copy right*/
	if (parameters.is_hdcp){
		asp_hdmi_dmac_reg_set_bit(ASP_HDMI_SIO_CH0_STATUS1_L, HDMI_SIO_HDCP_BIT);
		asp_hdmi_dmac_reg_set_bit(ASP_HDMI_SIO_CH0_STATUS1_R, HDMI_SIO_HDCP_BIT);
		dev_info(priv->dev, "[%s:%d],set sio hdcp bit to 1.\n", __FUNCTION__, __LINE__);
	}

	/* set bitwidth @TODO: you must modify this if used dynamic bitdth because now configure reg according to 16bit */
	if (parameters.tx3_conf->bit_width < BIT_WIDTH_MAX){
		reg_value = asp_hdmi_dmac_reg_read(ASP_HDMI_SIO_CH0_STATUS2_L);
		reg_value |=  HDMI_SIO_BITWIDTH_MASK;
		asp_hdmi_dmac_reg_write(ASP_HDMI_SIO_CH0_STATUS2_L, reg_value);
		asp_hdmi_dmac_reg_write(ASP_HDMI_SIO_CH0_STATUS2_R, reg_value);
		dev_info(priv->dev, "[%s:%d],set sio bitwith(%d).\n", __FUNCTION__, __LINE__, parameters.tx3_conf->bit_width);
	}
	else{
		dev_info(priv->dev, "[%s:%d],bitwith(%d) is no support, and will use defaulte sio bitwidth config.\n", __FUNCTION__, __LINE__, parameters.tx3_conf->bit_width);
	}

	dev_info(priv->dev, "[%s:%d],set ASP_HDMI_SIO_CH0_STATUS1_L vaule:%x\n", __FUNCTION__, __LINE__, reg_value = asp_hdmi_dmac_reg_read(ASP_HDMI_SIO_CH0_STATUS1_L));
	dev_info(priv->dev, "[%s:%d],set ASP_HDMI_SIO_CH0_STATUS2_Lvaule:%x\n", __FUNCTION__, __LINE__, reg_value = asp_hdmi_dmac_reg_read(ASP_HDMI_SIO_CH0_STATUS2_L));
	dev_info(priv->dev, "[%s:%d],set ASP_HDMI_SIO_CH0_STATUS1_R vaule:%x\n", __FUNCTION__, __LINE__, reg_value = asp_hdmi_dmac_reg_read(ASP_HDMI_SIO_CH0_STATUS1_R));
	dev_info(priv->dev, "[%s:%d],set ASP_HDMI_SIO_CH0_STATUS2_R vaule:%x\n", __FUNCTION__, __LINE__, reg_value = asp_hdmi_dmac_reg_read(ASP_HDMI_SIO_CH0_STATUS2_R));

}

bool asp_hdmi_dma_is_stop(void)
{
	unsigned int dma_en = 0;
	unsigned int int_mask_flag = 0;

	dma_en = asp_hdmi_dmac_reg_read(ASP_HDMI_DMA_EN);
	int_mask_flag = asp_hdmi_dmac_reg_read(ASP_HDMI_INT_MSK_STATE);

	if ((dma_en | int_mask_flag) & HDMI_DMA_EN_MASK) {
		return false;
	}

	return true;
}

int asp_hdmi_dma_config(
				unsigned int addr,
				unsigned int size)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;

	BUG_ON(NULL == priv);

	if ((0 != (addr & HDMI_DMA_ADD_VALID_MASK)) || (0 != (size & HDMI_DMA_ADDLEN_VALID_MASK))) {
		dev_err(priv->dev, "[%s:%d]   parame invalid:addr= %pK ; size =%ud\n", __func__, __LINE__,  (void *)(unsigned long)addr, size);
		return -EINVAL;
	}

	/* clean up i2s configuration */
	asp_hdmi_dmac_reg_write(ASP_HDMI_I2S_CLR, HDMI_I2S_CLR_MASK);

	/* selete i2s interface as ASP_SPDIFSEL */
	asp_hdmi_dmac_reg_clr_bit(ASP_HDMI_SPDIF_SEL, 0);

	/* set PCMA src addr and buffer */
	asp_hdmi_dmac_reg_write(ASP_HDMI_A_ADDR, addr);
	asp_hdmi_dmac_reg_write(ASP_HDMI_A_LEN, size);

	/* set PCMB src addr and buffer */
	asp_hdmi_dmac_reg_write(ASP_HDMI_B_ADDR, addr + size);
	asp_hdmi_dmac_reg_write(ASP_HDMI_B_LEN, size);

	/* clean up the irq */
	asp_hdmi_dmac_reg_write(ASP_HDMI_INT_CLR, HDMI_INT_MASK);

	dev_info(priv->dev, "dma config succ.\n");

	return 0;
}

void asp_hdmi_dma_clear_interrupt(unsigned int value)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;

	BUG_ON(NULL == priv);

	asp_hdmi_dmac_reg_write(ASP_HDMI_INT_CLR, value);
}

int asp_hdmi_dma_start(void)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;

	BUG_ON(NULL == priv);

	/* dma enable */
	asp_hdmi_dmac_reg_write(ASP_HDMI_DMA_EN, HDMI_DMA_EN_MASK);

	/* dma interrupt enable */
	asp_hdmi_dmac_reg_write(ASP_HDMI_INT_EN, HDMI_INT_MASK);

	/* set i2s */
	asp_hdmi_dmac_reg_write(ASP_HDMI_I2S_SET, HDMI_I2S_SET_MASK);

#ifdef ASP_HDMI_DMA_DEBUG
	asp_hdmi_dmac_dump();
#endif

	dev_info(priv->dev, "hdmi dma start succ.\n");

	return 0;
}

void asp_hdmi_dma_stop(void)
{
	struct asp_hdmi_dma_priv *priv = asp_hdmi_dma_priv;

	BUG_ON(NULL == priv);

	/* dma interrupt disable */
	asp_hdmi_dmac_reg_clr_bit(ASP_HDMI_INT_EN, 0);
	asp_hdmi_dmac_reg_clr_bit(ASP_HDMI_INT_EN, 1);
	asp_hdmi_dmac_reg_clr_bit(ASP_HDMI_INT_EN, 2);

	/* dma  disable */
	asp_hdmi_dmac_reg_write(ASP_HDMI_DMA_EN, HDMI_DMA_DISABLE_MASK);

	dev_info(priv->dev, "hdmi dma stop succ\n");
}

static int asp_hdmi_dma_probe(struct platform_device *pdev)
{
	struct device *dev = NULL;
	struct asp_hdmi_dma_priv *priv = NULL;

	if (!pdev) {
		printk(KERN_ERR"[%s:%d]  pdev is NULL!\n", __func__, __LINE__);
		return -EINVAL;
	}

	dev = &pdev->dev;
	dev_info(dev, "probe begin.\n");

	priv = devm_kzalloc(dev, sizeof(struct asp_hdmi_dma_priv), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "[%s:%d]  malloc failed.\n", __func__, __LINE__);
		return -ENOMEM;
	}

	priv->res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!priv->res) {
		dev_err(dev, "[%s:%d]  get resource failed.\n", __func__, __LINE__);
		return -ENOENT;
	}

	priv->asp_dma_reg_base_addr = devm_ioremap(dev, priv->res->start, resource_size(priv->res));
	if (!priv->asp_dma_reg_base_addr) {
		dev_err(dev, "[%s:%d]  asp dma reg addr ioremap failed.\n", __func__, __LINE__);
		return -ENOMEM;
	}

#ifdef ASP_HDMI_DMA_DEBUG
	dev_dbg(dev, "res->start.%pK \n", (void *)priv->res->start);
	dev_dbg(dev, "asp_dma_reg_base_addr.%pK \n", (void *)priv->asp_dma_reg_base_addr);
#endif

	wake_lock_init(&priv->wake_lock, WAKE_LOCK_SUSPEND, "asp_hdmi_dma");

	spin_lock_init(&priv->lock);

	priv->dev = dev;

	platform_set_drvdata(pdev, priv);

	asp_hdmi_dma_priv = priv;

	dev_info(dev, "probe end.\n");

	return 0;
}

static int asp_hdmi_dma_remove(struct platform_device *pdev)
{
	struct asp_hdmi_dma_priv *priv =
		(struct asp_hdmi_dma_priv *)platform_get_drvdata(pdev);

	if (!priv)
		return 0;

	wake_lock_destroy(&priv->wake_lock);

	asp_hdmi_dma_priv = NULL;

	dev_info(priv->dev, "asp dma driver remove succ.\n");

	return 0;
}

static const struct of_device_id of_asp_hdmi_dma_match[] = {
	{ .compatible = "hisilicon,asp-hdmi-dma", },
	{},
};

MODULE_DEVICE_TABLE(of, of_asp_hdmi_dma_match);

static struct platform_driver asp_hdmi_dma_driver = {
		.driver    = {
		.name      = "asp_hdmi_dma_drv",
		.owner     = THIS_MODULE,
		.of_match_table = of_asp_hdmi_dma_match,
	},
	.probe    = asp_hdmi_dma_probe,
	.remove   = asp_hdmi_dma_remove,
};

static int __init asp_hdmi_dma_init(void)
{
	return platform_driver_register(&asp_hdmi_dma_driver);
}
module_init(asp_hdmi_dma_init);

static void __exit asp_hdmi_dma_exit(void)
{
	platform_driver_unregister(&asp_hdmi_dma_driver);
}
module_exit(asp_hdmi_dma_exit);

MODULE_AUTHOR("LiuPan <liupan21@huawei.com>");
MODULE_DESCRIPTION("Hisilicon (R) ASP HDMI DMA Driver");
MODULE_LICENSE("GPL");
