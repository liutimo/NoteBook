静态IP动态IP设置流程

1. 相关文件

    ```java
    EthernetNetworkFactory.java
    ConnectivityService.java
    DhcpClient.java
    IpManager.java
    EthernetSettings.java
    ```







## DHCP失败时设置静态IP，但不影响Settings



1. EthernetNetworkFactory::startIpManager();

    if DHCP:

    ​	初始化IPManager状态机。 对应CMD_START

    

    然后DHCPClient运行，如果获取到IP调用：

    ​	IpManager.Callback.onProvisioningSuccess

    没有获取到IP：继续运行？

    

    

    

    

    

    

# IpManager

一个状态机。

```java
private final State mStoppedState = new StoppedState();
private final State mStoppingState = new StoppingState();
private final State mStartedState = new StartedState();
private final State mRunningState = new RunningState();
```



状态机初始化

```java
private void configureAndStartStateMachine() {
	addState(mStoppedState);
	addState(mStartedState);
		addState(mRunningState, mStartedState);
	addState(mStoppingState);
    //初始化状态
	setInitialState(mStoppedState);
	super.start();
}
```

1. `StoppedState`

    enter该状态后，完成清理工作。清除相应net interface的信息。`mNwService`

    

    

    

