/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define DEBUG
#define LOG_FLAG	"sipa_aux"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <sound/core.h>
#include <sound/soc.h>

#include "sipa_common.h"

unsigned int soc_sia81xx_get_aux_num(
	struct platform_device *pdev)
{
	int ret = 0;
	u32 max_dev_num = 0, dev_num = 0;

	if (NULL == pdev) {
		pr_err("[  err][%s] : NULL == pdev !!! \r\n",
			__func__);
		return 0;
	}

	if (NULL == pdev->dev.of_node) {
		pr_err("[  err][%s] : NULL == pdev->dev.of_node !!! \r\n",
			__func__);
		return 0;
	}

	ret = of_property_read_u32(pdev->dev.of_node,
				"si,sia81xx-max-num", &max_dev_num);
	if (0 != ret) {
		pr_err("[  err][%s] : of_property_read_u32 ret = %d !!! \r\n",
			__func__, ret);
		return 0;
	}

	if (0 == max_dev_num) {
		pr_warn("[ warn][%s] : max_dev_num = %u !!! \r\n",
			__func__, max_dev_num);
		return 0;
	}

	/* Get count of WSA device phandles for this platform */
	dev_num = of_count_phandle_with_args(pdev->dev.of_node,
					"si,sia81xx-aux-devs", NULL);
	if (0 >= dev_num) {
		pr_warn("[ warn][%s] : dev_num = %u !!! \r\n",
			__func__, dev_num);
		return 0;
	}

	if (dev_num > max_dev_num)
		dev_num = max_dev_num;

	return (unsigned int)dev_num;
}
EXPORT_SYMBOL(soc_sia81xx_get_aux_num);

unsigned int soc_sia81xx_get_codec_conf_num(
	struct platform_device *pdev)
{
	return soc_sia81xx_get_aux_num(pdev);
}
EXPORT_SYMBOL(soc_sia81xx_get_codec_conf_num);

int soc_sia81xx_init(
	struct platform_device *pdev,
	struct snd_soc_aux_dev *aux_dev,
	u32 aux_num,
	struct snd_soc_codec_conf *codec_conf,
	u32 conf_num)
{
	int ret, i;
	u32 dev_num = 0, prefix_num = 0;
	struct device_node *dev_of_node;
	const char *dev_name_prefix = NULL;
	char *aux_dev_name = NULL;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 3, 18))
	sipa_dev_t *sia81xx = NULL;
#endif

	if ((0 == aux_num) || (0 == conf_num)) {
		pr_err("[  err][%s] : aux_num = %u, config_num= %u \r\n",
			__func__, aux_num, conf_num);
		return -EINVAL;
	}

	dev_num = soc_sia81xx_get_aux_num(pdev);
	if ((aux_num != dev_num) || (conf_num != dev_num)) {
		pr_err("[  err][%s] : aux_num = %u, config_num= %u, "
			"dev_num = %u !!! \r\n",
			__func__, aux_num, conf_num, dev_num);
		return -EINVAL;
	}

	prefix_num = of_property_count_strings(pdev->dev.of_node,
						"si,sia81xx-aux-devs-prefix");
	if (dev_num > prefix_num) {
		pr_err("[  err][%s] : dev_num = %u, prefix_num = %u !!! \r\n",
			__func__, dev_num, prefix_num);
		return -EINVAL;
	}

	for (i = 0; i < dev_num; i++) {
		dev_of_node = of_parse_phandle(pdev->dev.of_node,
							"si,sia81xx-aux-devs", i);
		if (unlikely(NULL == dev_of_node)) {
			pr_warn("[ warn][%s]: sia81xx dev %d parse error !!! \r\n",
				__func__, i);
			return  -EINVAL;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
							"si,sia81xx-aux-devs-prefix",
							i, &dev_name_prefix);
		if (0 != ret) {
			pr_warn("[ warn][%s] : sia81xx dev %d "
				"parse prefix ret = %d !!! \r\n",
				__func__, i, ret);
			return ret;
		}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 3, 18))
		sia81xx = find_sipa_dev(dev_of_node);
		if (NULL == sia81xx) {
			pr_err("[  err][%s] %s: sia81xx_dev not find\n",
				LOG_FLAG, __func__);
			return -EINVAL;
		}

		aux_dev_name = devm_kstrdup(&pdev->dev, (const char *)dev_name(&sia81xx->pdev->dev), GFP_KERNEL);

		aux_dev[i].dlc.name = (const char *)aux_dev_name;
		aux_dev[i].dlc.dai_name = NULL;
		aux_dev[i].dlc.of_node = dev_of_node;
#else
		aux_dev_name = devm_kzalloc(&pdev->dev, 128, GFP_KERNEL);
		if (NULL == aux_dev_name) {
			/*
				* FIXED ME : before i, the aux_dev_pos[i].name's memory
				* didn't free !!!
				*/
			return -ENOMEM;
		}
		snprintf(aux_dev_name, strlen("sia81xx.%d"), "sia81xx.%d", i);

		aux_dev[i].name = (const char *)aux_dev_name;
		aux_dev[i].codec_name = NULL;
		aux_dev[i].codec_of_node = dev_of_node;
#endif
		aux_dev[i].init = NULL;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 5, 19))
		codec_conf[i].dlc.name = NULL;
		codec_conf[i].dlc.of_node = dev_of_node;
#else
		codec_conf[i].dev_name = NULL;
		codec_conf[i].of_node = dev_of_node;
#endif
		codec_conf[i].name_prefix = dev_name_prefix;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 3, 18))
		pr_debug("[debug][%s] : aux_dev = %s \r\n",
			__func__, aux_dev[i].dlc.name);
#else
		pr_debug("[debug][%s] : aux_dev = %s \r\n",
			__func__, aux_dev[i].name);
#endif
	}

	return 0;
}
EXPORT_SYMBOL(soc_sia81xx_init);

int soc_sia81xx_deinit(
	struct platform_device *pdev,
	struct snd_soc_aux_dev *aux_dev,
	u32 aux_num,
	struct snd_soc_codec_conf *codec_conf,
	u32 conf_num)
{
	int i = 0;

	if (NULL == pdev) {
		pr_err("[  err][%s] : NULL == pdev \r\n", __func__);
		return -EINVAL;
	}

	if ((NULL != aux_dev) && (aux_num > 0)) {
		for (i = 0; i < aux_num; i++) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 3, 18))
			if (NULL != aux_dev[i].dlc.name) {
				devm_kfree(&pdev->dev, (char *)aux_dev[i].dlc.name);
				aux_dev[i].dlc.name = NULL;
			}
#else
			if (NULL != aux_dev[i].name) {
				devm_kfree(&pdev->dev, (char *)aux_dev[i].name);
				aux_dev[i].name = NULL;
			}
#endif
		}
	}

	return 0;
}
EXPORT_SYMBOL(soc_sia81xx_deinit);

int soc_aux_init_only_sia81xx(
	struct platform_device *pdev,
	struct snd_soc_card *card)
{
	int ret = 0;
	u32 aux_num = 0, conf_num = 0;
	struct snd_soc_aux_dev *aux_dev = NULL;
	struct snd_soc_codec_conf *codec_conf = NULL;

	aux_num = soc_sia81xx_get_aux_num(pdev);
	conf_num = soc_sia81xx_get_codec_conf_num(pdev);

	if ((aux_num != conf_num) || (0 == aux_num) || (NULL == card->dev)) {
		pr_err("[  err][%s] : aux_num = %u, conf_num= %u !!! \r\n",
			__func__, aux_num, conf_num);
		return -EINVAL;
	}

	/* make sure that the "dev_num" must be greater than 0 !!! */
	aux_dev = devm_kzalloc(card->dev,
					aux_num * sizeof(struct snd_soc_aux_dev),
					GFP_KERNEL);
	if (NULL == aux_dev)
		return -ENOMEM;

	codec_conf = devm_kzalloc(card->dev,
					conf_num * sizeof(struct snd_soc_codec_conf),
					GFP_KERNEL);
	if (NULL == codec_conf)
		return -ENOMEM;

	ret = soc_sia81xx_init(pdev, aux_dev, aux_num, codec_conf, conf_num);
	if (0 != ret) {
		pr_err("[  err][%s] : soc_sia81xx_init ret = %d !!! \r\n",
			__func__, ret);
		return ret;
	}

	card->aux_dev = aux_dev;
	card->codec_conf = codec_conf;

	card->num_aux_devs = aux_num;
	card->num_configs = conf_num;

	return 0;
}
EXPORT_SYMBOL(soc_aux_init_only_sia81xx);

int soc_aux_deinit_only_sia81xx(
	struct platform_device *pdev,
	struct snd_soc_card *card)
{
	if (NULL == pdev) {
		pr_err("[  err][%s] : NULL == pdev \r\n", __func__);
		return -EINVAL;
	}

	if (NULL == card) {
		pr_err("[  err][%s] : NULL == card \r\n", __func__);
		return -EINVAL;
	}

	soc_sia81xx_deinit(
		pdev,
		card->aux_dev,
		card->num_aux_devs,
		card->codec_conf,
		card->num_configs);

	if (NULL != card->aux_dev) {
		devm_kfree(&pdev->dev, card->aux_dev);
		card->aux_dev = NULL;
	}

	if (NULL != card->codec_conf) {
		devm_kfree(&pdev->dev, card->codec_conf);
		card->codec_conf = NULL;
	}

	card->num_aux_devs = 0;
	card->num_configs = 0;

	return 0;
}
EXPORT_SYMBOL(soc_aux_deinit_only_sia81xx);


int soc_sia91xx_init(
	struct platform_device *pdev,
	struct snd_soc_codec_conf *codec_conf,
	u32 conf_num)
{
	int ret, i;
	u32 dev_num = 0, prefix_num = 0;
	struct device_node *dev_of_node;
	const char *dev_name_prefix = NULL;

	if (0 == conf_num) {
		pr_err("[  err][%s] : config_num= %u \r\n",
			__func__, conf_num);
		return -EINVAL;
	}

	dev_num = soc_sia81xx_get_aux_num(pdev);
	if (conf_num != dev_num) {
		pr_err("[  err][%s] : config_num= %u,dev_num = %u !!! \r\n",
			__func__, conf_num, dev_num);
		return -EINVAL;
	}

	prefix_num = of_property_count_strings(pdev->dev.of_node,
						"si,sia81xx-aux-devs-prefix");
	if (dev_num > prefix_num) {
		pr_err("[  err][%s] : dev_num = %u, prefix_num = %u !!! \r\n",
			__func__, dev_num, prefix_num);
		return -EINVAL;
	}

	for (i = 0; i < dev_num; i++) {
		dev_of_node = of_parse_phandle(pdev->dev.of_node,
							"si,sia81xx-aux-devs", i);
		if (unlikely(NULL == dev_of_node)) {
			pr_warn("[ warn][%s]: sia81xx dev %d parse error !!! \r\n",
				__func__, i);
			return  -EINVAL;
		}

		ret = of_property_read_string_index(pdev->dev.of_node,
							"si,sia81xx-aux-devs-prefix",
							i, &dev_name_prefix);
		if (0 != ret) {
			pr_warn("[ warn][%s] : sia81xx dev %d "
				"parse prefix ret = %d !!! \r\n",
				__func__, i, ret);
			return ret;
		}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(5, 5, 19))
		codec_conf[i].dlc.name = NULL;
		codec_conf[i].dlc.of_node = dev_of_node;
#else
		codec_conf[i].dev_name = NULL;
		codec_conf[i].of_node = dev_of_node;
#endif
		codec_conf[i].name_prefix = dev_name_prefix;
	}

	return 0;
}
EXPORT_SYMBOL(soc_sia91xx_init);

int soc_codec_conf_sia91xx(
	struct platform_device *pdev,
	struct snd_soc_card *card)
{
	int ret = 0;
	u32 conf_num = 0;
	struct snd_soc_codec_conf *codec_conf = NULL;

	conf_num = soc_sia81xx_get_codec_conf_num(pdev);
	if (conf_num == 0) {
		return ret;
	}

	codec_conf = devm_kzalloc(card->dev,
					conf_num * sizeof(struct snd_soc_codec_conf),
					GFP_KERNEL);
	if (NULL == codec_conf)
		return -ENOMEM;

	ret = soc_sia91xx_init(pdev, codec_conf, conf_num);
	if (0 != ret) {
		pr_err("[  err][%s] : soc_sia91xx_init ret = %d !!! \r\n",
			__func__, ret);
		return ret;
	}

	card->codec_conf = codec_conf;
	card->num_configs = conf_num;

	return 0;
}
EXPORT_SYMBOL(soc_codec_conf_sia91xx);

int soc_codec_conf_sipa(
	struct platform_device *pdev,
	struct snd_soc_card *card) {

	int ret = 0;
	u32 is_digital_pa = 0;

	ret = of_property_read_u32(pdev->dev.of_node,
				"si,is_digital_pa", &is_digital_pa);
	if (0 != ret) {
		pr_info("[ info][%s] : node is not exist. ret = %d !!! \r\n", __func__, ret);
	}

	if (is_digital_pa) {
		ret = soc_codec_conf_sia91xx(pdev, card);
	} else {
		ret = soc_aux_init_only_sia81xx(pdev, card);
	}

	return ret;
}
EXPORT_SYMBOL(soc_codec_conf_sipa);
