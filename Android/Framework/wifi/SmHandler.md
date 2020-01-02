1. `processMsg(Message msg)`

    获取当前状态处理`Message`,如果当前状态不能处理 `Message`，就用当前状态的父状态处理。

    ```java
    private final State processMsg(Message msg) {
        StateInfo curStateInfo = mStateStack[mStateStackTopIndex];
    
        if (isQuit(msg)) {
            transitionTo(mQuittingState);
        } else {
    		while (!curStateInfo.state.processMessage(msg)) {
                /**
                 * Not processed
                 */
                curStateInfo = curStateInfo.parentStateInfo;
                if (curStateInfo == null) {
                    /**
                     * No parents left so it's not handled
                     */
                    mSm.unhandledMessage(msg);
                    break;
                }
                if (mDbg) {
                    mSm.log("processMsg: " + curStateInfo.state.getName());
                }
            }
        }
        return (curStateInfo != null) ? curStateInfo.state : null;
    }
    ```

2. `performTransitions(State msgProcessedState, Message msg)`

    完成状态切换