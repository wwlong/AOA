## raspberry pi Android Accessory

### 说明
*   Raspberry Pi作为Android Accessory与Android设备进行通信
    *   通过USB接收来自Android设备的request,例如开门,WARNING等命令
    *   通过PIR的结果,将有人靠近的信息通过USB传输到Android设备

### Raspberry的GPIO分配
    *   点亮LED灯
        *   GPIO17 -- 开门提醒
        *   GPIO18 -- warning
    *   开门信号
        *   GPIO26 -- 开门
