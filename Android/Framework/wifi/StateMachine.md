# StateMachine



## State

`enter`和`exit`类似于构造函数和析构函数，分别用于初始化和清理状态。

子类通过实现`processMessage`来处理指定的消息。

```java
public class State implements IState {
    protected State() {
    }

    @Override
    public void enter() {
    }

    
    @Override
    public void exit() {
    }

    @Override
    public boolean processMessage(Message msg) {
        return false;
    }
    @Override
    public String getName() {
        String name = getClass().getName();
        int lastDollar = name.lastIndexOf('$');
        return name.substring(lastDollar + 1);
    }
}

```



# StateMachine

当状态机被创建时，通过`addState`可以建立一个具备层次关系的状态树。通过`setInitialState`可以指定一个初始状态。`start`函数会初始化并启动状态机，状态机启动后的第一个动作就是调用初始状态的`enter`函数。

`sendMessage`用于发送message。当状态机接收到Message时，会调用当前状态的`processMessage`来处理消息。`transitionTo`用于切换当前状态。



状态机中的每一个状态都有0/1个父状态。当Message不能被当前状态处理时，就会使用其父状态处理。如果没有状态能够处理该Message。`unhandledMessage`就会被调用。