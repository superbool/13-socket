
stm32 lwip相关记录

参考笔记：
* https://www.kancloud.cn/jiejietop/tcpip/988507
* https://www.st.com/resource/en/user_manual/um1713-developing-applications-on-stm32cube-with-lwip-tcpip-stack-stmicroelectronics.pdf
* https://github.com/fboris/STM32Cube_FW_F4/blob/master/Projects/STM324xG_EVAL/Applications/LwIP/LwIP_TCP_Echo_Server/Src/tcp_echoserver.c
* https://doc.embedfire.com/net/lwip/zh/latest/doc/chapter16/chapter16.html

正点原子stm42f4开发版上PHY芯片LAN8720A与cubeMX上芯片LAN8742A功能一致，但需要注意的点： 
## 1 cubeMX自动生成的PHY引脚跟实际的开发板引脚有不同，需要手动调整下，开发板引脚：
```
    PC1     ------> ETH_MDC
    PA1     ------> ETH_REF_CLK
    PA2     ------> ETH_MDIO
    PA7     ------> ETH_CRS_DV
    PC4     ------> ETH_RXD0
    PC5     ------> ETH_RXD1
    PG11     ------> ETH_TX_EN
    PG13     ------> ETH_TXD0
    PG14     ------> ETH_TXD1
```
 

## 2 注意手动添加LAN8720A的RESET引脚以及相关的初始化reset代码，cubeMX生成的代码不含RESET相关引脚及代码（如果不加reset，芯片无法正常工作）
RESET引脚对接芯片的GPIO-PE2
ethernetif.c -> HAL_ETH_MspInit函数添加：
```
  HAL_GPIO_WritePin(ETH_RESET_GPIO_Port,ETH_RESET_Pin,GPIO_PIN_RESET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(ETH_RESET_GPIO_Port,ETH_RESET_Pin,GPIO_PIN_SET);
  HAL_Delay(100);
```

## 3 当开启DHCP时，启动阶段芯片PHYLinkState可能尚未初始化完毕，此时MX_LWIP_Init不会netif_set_up，这时如果启动dhcp将会报异常
“netif is not up, old style port?”, 且DHCP没有实际启动。
为了解决这个问题，在netif_set_link_callback的回调函数里ethernet_link_status_updated添加判断netif_is_up后重新启动dhcp
```
dhcp_start(&gnetif); 
``` 
此时可以热插拔网线都可以再次获取到ip, 注意在没有RTOS情况下，需要在主while循环中添加 *MX_LWIP_Process();* 它会轮训状态重新启动dhcp.
在freeRTOS中会自动添加状态检查的任务：`MX_LWIP_Init`自动生成有`osThreadNew(ethernet_link_thread, &gnetif, &attributes);`


## 4 在有freeRTOS时对于创建的任务stacksize必须要大于代码中要创建的内存，否则会出现各种异常，包括不限于：
* 加了freeRTOS后，上面第三点中加了dhcp_start后，会导致系统死机，需要把MX_LWIP_Init中的dhcp_start注释掉就可以了（实际不用注释），原因是堆栈设置太小了
* 自定义tcp server，只能接受一个链接，且加了write后系统启动就死机，不加的话只能接收一个连接，堆栈设置太小了


## 关于LAN8720A芯片的一些简单说明
https://blog.csdn.net/sinat_20265495/article/details/79628283

1 PHY地址
    PHYAD0配置PHY地址，如果不接(内部下拉)，则对应SMI地址是0

2 nINT/REFCLKO引脚功能
    根据原理图，LAN8720A LED2/nINTSEL引脚接下拉电阻，所以对应nINT/REFCLKO引脚是REF_CLK OUT模式，也就是参数时钟输出
    如果LED2/nINTSEL不接(内部上拉)或者接上拉电阻，则nINT/REFCLKO引脚对应的是中断输出
    PHY的时钟由外部25M晶振提供，通过REF_CLK输出给MAC

3 内部1.2v电压配置
    LED1/REGOFF接地使用内部1.2v稳压器


## 实际通信方式

1 采用光纤模块直连单片机和电脑

2 单片机和电脑均设置为静态ip,注意ip尾缀不同，子网掩码和Gateway一致
