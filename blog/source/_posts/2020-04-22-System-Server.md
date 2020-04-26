---
title: System Server
date: 2020-04-22 15:03:37
categories: Android Framework
tags: 
    - Android Framework
    - System Server	
---

## SystemServer的启动

`Zygote`启动后，会根据init.rc中的`--start-system-server`来启动SystemServer进程。



### SystemServer的启动 -- Native

在`ZygoteInit`的main函数中，有这么一段代码:	

```c++
// # frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
if (startSystemServer) {
	Runnable r = forkSystemServer(abiList, socketName, zygoteServer);
	// {@code r == null} in the parent (zygote) process, and {@code r != null} in the
	// child (system_server) process.
	if (r != null) {
		//run内部实际就是执行com.android.server.SystemServer.Main()函数
		//这里，就进入了SystemServer的逻辑了
		r.run();
	return;
	}
}
```

这里，就完成了`SystemServer`进程的创建。

```java
// # frameworks/base/core/java/com/android/internal/os/ZygoteInit.java
private static Runnable forkSystemServer(String abiList, String socketName,
		ZygoteServer zygoteServer) {
    //[1]
	long capabilities = posixCapabilitiesAsBits(
			OsConstants.CAP_IPC_LOCK,
			OsConstants.CAP_KILL,
			OsConstants.CAP_NET_ADMIN,
			OsConstants.CAP_NET_BIND_SERVICE,
			OsConstants.CAP_NET_BROADCAST,
			OsConstants.CAP_NET_RAW,
			OsConstants.CAP_SYS_MODULE,
			OsConstants.CAP_SYS_NICE,
			OsConstants.CAP_SYS_PTRACE,
			OsConstants.CAP_SYS_TIME,
			OsConstants.CAP_SYS_TTY_CONFIG,
			OsConstants.CAP_WAKE_ALARM
			);
	/* Containers run without this capability, so avoid setting it in that case */
	if (!SystemProperties.getBoolean(PROPERTY_RUNNING_IN_CONTAINER, false)) {
		capabilities |= posixCapabilitiesAsBits(OsConstants.CAP_BLOCK_SUSPEND);
	}
	/* Hardcoded command line to start the system server */
	String args[] = {
		"--setuid=1000",
		"--setgid=1000",
		"--setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,1032,3001,3002,3003,3006,3007,3009,3010",
		"--capabilities=" + capabilities + "," + capabilities,
		"--nice-name=system_server",
		"--runtime-args",
		"com.android.server.SystemServer",
	};
    
    //[2]
	ZygoteConnection.Arguments parsedArgs = null;

	int pid;

	try {
		parsedArgs = new ZygoteConnection.Arguments(args);
		ZygoteConnection.applyDebuggerSystemProperty(parsedArgs);
		ZygoteConnection.applyInvokeWithSystemProperty(parsedArgs);

		/* Request to fork the system server process */
        //[3]
		pid = Zygote.forkSystemServer(
				parsedArgs.uid, parsedArgs.gid,
				parsedArgs.gids,
				parsedArgs.debugFlags,
				null,
				parsedArgs.permittedCapabilities,
				parsedArgs.effectiveCapabilities);
	} catch (IllegalArgumentException ex) {
		throw new RuntimeException(ex);
	}

	/* For child process */
	if (pid == 0) {
		//等待第二个zygote进程启动。
		if (hasSecondZygote(abiList)) {
			waitForSecondaryZygote(socketName);
		}
		//关闭继承至父进程的socket
		zygoteServer.closeServerSocket();
		//[4]
		return handleSystemServerProcess(parsedArgs);
	}
	return null;
}

```

`[1]`处代码，最终通过调用`capset`函数来设置指定好的能力集参数。

`[2]`处代码，通过`ZygoteConnection.Arguments`来解析`args`字符串。进程创建过程中，会根据这些参数来设置`uid`、`guid`以及补充用户组等信息。特别注意`com.android.server.SystemServer`，其会赋值给`ZygoteConnection.Arguments.remainingArgs`，后面就是根据这个参数来进入到启动SystemServer的main函数中的。

`[3]`处代码，进入到native的进程创建环节。

`[4]`处代码，完成systemServer fork后的剩余操作。



#### Zygote.forkSystemServer

```java
// # frameworks/base/core/java/com/android/internal/os/Zygote.java
public static int forkSystemServer(int uid, int gid, int[] gids, int debugFlags,
    		int[][] rlimits, long permittedCapabilities, long effectiveCapabilities) {
    // Resets nice priority for zygote process.
    resetNicePriority();
    int pid = nativeForkSystemServer(
        uid, gid, gids, debugFlags, rlimits, permittedCapabilities, effectiveCapabilities);
    return pid;
}
```

`uid`和`gid`设置为1000。

`gids`对应如下，通过调用`setgroups`设置。

```shell
setgroups=1001,1002,1003,1004,1005,1006,1007,1008,1009,1010,1018,1021,1023,1032,3001,3002,3003,3006,3007,3009,3010
```

`permittedCapabilities`和`effectiveCapabilities`，看代码，感觉这两个参数的值是一样的。

```java
String[] capStrings = capString.split(",", 2);
if (capStrings.length == 1) {
	effectiveCapabilities = Long.decode(capStrings[0]);
	permittedCapabilities = effectiveCapabilities;
} else { 
    //"--capabilities=" + capabilities + "," + capabilities, 
    //走到这个分支了咯
	permittedCapabilities = Long.decode(capStrings[0]);
	effectiveCapabilities = Long.decode(capStrings[1]);
}
```

最终通过`setcap`设置下去。

`rlimits == null `  ，具体含义分析APP启动时再看。



```c++
// #frameworks/base/core/jni/com_android_internal_os_Zygote.cpp
static jint com_android_internal_os_Zygote_nativeForkSystemServer(
        JNIEnv* env, jclass, uid_t uid, gid_t gid, jintArray gids,
        jint debug_flags, jobjectArray rlimits, jlong permittedCapabilities,
        jlong effectiveCapabilities) {
  pid_t pid = ForkAndSpecializeCommon(env, uid, gid, gids,
                                      debug_flags, rlimits,
                                      permittedCapabilities, effectiveCapabilities,
                                      MOUNT_EXTERNAL_DEFAULT, NULL, NULL, true, NULL,
                                      NULL, NULL, NULL);
  if (pid > 0) {
      gSystemServerPid = pid;
      int status;
      if (waitpid(pid, &status, WNOHANG) == pid) {
          RuntimeAbort(env, __LINE__, "System server process has died. Restarting Zygote!");
      }

      // Assign system_server to the correct memory cgroup.
      if (!WriteStringToFile(StringPrintf("%d", pid), "/dev/memcg/system/tasks")) {
      }
  }
  return pid;
}
```

`ForkAndSpecializeCommon`实现有点长，而且不仅仅是针对`SystemServer`进程，所以，这里截取片段来说明。

##### Zygote 孵化 SystemServer前的信号处理函数

在Native创建SystemServer，首先值得关注的就是`SetSigChldHandler`。

```c++
ForkAndSpecializeCommon() {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SigChldHandler;
    int err = sigaction(SIGCHLD, &sa, NULL);
    
    forK();
}
static void SigChldHandler(int /*signal_number*/) {
  pid_t pid;
  int status;
  int saved_errno = errno;

  while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
    if (WIFEXITED(status)) {
      if (WEXITSTATUS(status)) {
        ALOGI("Process %d exited cleanly (%d)", pid, WEXITSTATUS(status));
      }
    } else if (WIFSIGNALED(status)) {
      if (WTERMSIG(status) != SIGKILL) {
        ALOGI("Process %d exited due to signal (%d)", pid, WTERMSIG(status));
      }
      if (WCOREDUMP(status)) {
        ALOGI("Process %d dumped core.", pid);
      }
    }
	//这里，如果SystemServer结束了，Zygote进程就会自我毁灭，然后init进程会重启Zygote。
    if (pid == gSystemServerPid) {
      ALOGE("Exit zygote because system server (%d) has terminated", pid);
      kill(getpid(), SIGKILL);
    }
  }
  errno = saved_errno;
}
```



##### 通过fork创建子进程

```c++
ForkAndSpecializeCommon() {
  pid_t pid = fork();
  if (pid == 0) {
    //子进程创建完毕，取消对SIGCHLD信号的屏蔽
    sigprocmask(SIG_UNBLOCK, &sigchld, nullptr);
    // Keep capabilities across UID change, unless we're staying root.
    if (uid != 0) {
      EnableKeepCapabilities(env);
    }

    SetInheritable(env, permittedCapabilities);
    DropCapabilitiesBoundingSet(env);
    //...
    if (!is_system_server) {
        //创建一个以进程组， 名字形式 /dev/memcg/apps/uid_1000/
        int rc = createProcessGroup(uid, getpid());
    }

    //设置补充组ID the supplementary group IDs
    SetGids(env, javaGids);
    SetRLimits(env, javaRlimits);
    int rc = setresgid(gid, gid, gid);
    rc = setresuid(uid, uid, uid);

    if (NeedsNoRandomizeWorkaround()) {
        // Work around ARM kernel ASLR lossage (http://b/5817320).
        int old_personality = personality(0xffffffff);
        int new_personality = personality(old_personality | ADDR_NO_RANDOMIZE);
        if (new_personality == -1) {
            ALOGW("personality(%d) failed: %s", new_personality, strerror(errno));
        }
    }
    //设置进程的能力集
    SetCapabilities(env, permittedCapabilities, effectiveCapabilities, permittedCapabilities);

    //设置进程调度优先级为default
    SetSchedulerPolicy(env);

    //取消SystemServer进程对SIGCHLD的handler
    UnsetSigChldHandler();

    //执行gZygoteClass对应的函数gCallPostForkChildHooks
    env->CallStaticVoidMethod(gZygoteClass, gCallPostForkChildHooks, debug_flags,
                              is_system_server, instructionSet);
  }
```

这里，值得注意的就是创建进程组。android在目录`/dev/memcg/apps`目下，会创建进程组的文件夹。目录结构就是

`uid_<uid>`：表示一个进程组。该目录下的`pid_<pid>`表示该进程组内的进程。



到这里，`fork`结束后，也就完成了`SystemServer`进程的创建。接下来就是调用其Java对应的`Main`函数了。



#### handleSystemServerProcess

创建SystemServer进城后，通过`handleSystemServerProcess`来加载`SystemServer`，并返回一个`Runnable`对象，通过调用这个对象的`run`方法来执行`SystemServer`的`main`方法。

```java
private static Runnable handleSystemServerProcess(ZygoteConnection.Arguments parsedArgs) {
	// 设置umask，进程创建的所有文件都会只有user位权限
	Os.umask(S_IRWXG | S_IRWXO);
	//最终调用 pthread_setname_np 设置线程名
	if (parsedArgs.niceName != null) {
		Process.setArgV0(parsedArgs.niceName);
	}
	
	//环境变量内容如下：
	// /system/framework/services.jar:/system/framework/ethernet-service.jar:
	// /system/framework/wifi-service.jar:/system/framework/com.android.location.provider.jar
	//这段代码具体左右不理解，大概就是优化dex文件？？？
	final String systemServerClasspath = Os.getenv("SYSTEMSERVERCLASSPATH");
	if (systemServerClasspath != null) {
		performSystemServerDexOpt(systemServerClasspath);
		boolean profileSystemServer = SystemProperties.getBoolean(
				"dalvik.vm.profilesystemserver", false);
		if (profileSystemServer && (Build.IS_USERDEBUG || Build.IS_ENG)) {
			try {
				File profileDir = Environment.getDataProfilesDePackageDirectory(
						Process.SYSTEM_UID, "system_server");
				File profile = new File(profileDir, "primary.prof");
				profile.getParentFile().mkdirs();
				profile.createNewFile();
				String[] codePaths = systemServerClasspath.split(":");
				VMRuntime.registerAppInfo(profile.getPath(), codePaths);
			} catch (Exception e) {
				Log.wtf(TAG, "Failed to set up system server profile", e);
			}
		}
	}

	//启动应用程序
	if (parsedArgs.invokeWith != null) {
		String[] args = parsedArgs.remainingArgs;
		if (systemServerClasspath != null) {
			String[] amendedArgs = new String[args.length + 2];
			amendedArgs[0] = "-cp";
			amendedArgs[1] = systemServerClasspath;
			System.arraycopy(args, 0, amendedArgs, 2, args.length);
			args = amendedArgs;
		}
		WrapperInit.execApplication(parsedArgs.invokeWith,
				parsedArgs.niceName, parsedArgs.targetSdkVersion,
				VMRuntime.getCurrentInstructionSet(), null, args);
	} else {
		//启动system server，最终会走到这个分支。
		ClassLoader cl = null;
		if (systemServerClasspath != null) {
			cl = createPathClassLoader(systemServerClasspath, parsedArgs.targetSdkVersion);
			Thread.currentThread().setContextClassLoader(cl);
		}
		//parsedArgs.remainingArgs = com.android.server.SystemServer
		return ZygoteInit.zygoteInit(parsedArgs.targetSdkVersion, parsedArgs.remainingArgs, cl);
	}
}

```

最终，调用` ZygoteInit.zygoteInit`启动`com.android.server.SystemServer`。

```java
public static final Runnable zygoteInit(int targetSdkVersion, String[] argv, ClassLoader classLoader) {
	RuntimeInit.commonInit();
	ZygoteInit.nativeZygoteInit();
	return RuntimeInit.applicationInit(targetSdkVersion, argv, classLoader);
}

private static final void commonInit() {
    // 设置默认的未捕捉异常处理方法
    Thread.setDefaultUncaughtExceptionHandler(new UncaughtHandler());

    // 设置市区，中国时区为"Asia/Shanghai"
    TimezoneGetter.setInstance(new TimezoneGetter() {
        @Override
        public String getId() {
            return SystemProperties.get("persist.sys.timezone");
        }
    });
    TimeZone.setDefault(null);

    //重置log配置
    LogManager.getLogManager().reset();
    new AndroidConfig();

    // 设置默认的HTTP User-agent格式,用于 HttpURLConnection。
    String userAgent = getDefaultUserAgent();
    System.setProperty("http.agent", userAgent);

    // 设置socket的tag，用于网络流量统计
    NetworkManagementSocketTagger.install();
}

//ZygoteInit.nativeZygoteInit();
static void com_android_internal_os_ZygoteInit_nativeZygoteInit(JNIEnv* env, jobject clazz)
{
    //frameworks/base/cmds/app_process/app_main.cpp
    //实际调用的是 AppRuntime->onZygoteInit
    gCurRuntime->onZygoteInit();
}

virtual void AppRuntime::onZygoteInit()
{
    //Binder相关。见binder
	sp<ProcessState> proc = ProcessState::self();
	proc->startThreadPool();
}
```

重头戏还是`RuntimeInit.applicationInit`

```java

protected static Runnable applicationInit(int targetSdkVersion, String[] argv,
            ClassLoader classLoader) {
	// Remaining arguments are passed to the start class's static main
	return findStaticMain(args.startClass, args.startArgs, classLoader);
 }

private static Runnable findStaticMain(String className, String[] argv,
            ClassLoader classLoader) {
    Class<?> cl = Class.forName(className, true, classLoader);
    Method m = cl.getMethod("main", new Class[] { String[].class });
    int modifiers = m.getModifiers();
    return new MethodAndArgsCaller(m, argv);
}

 static class MethodAndArgsCaller implements Runnable {
        /** method to call */
        private final Method mMethod;

        /** argument array */
        private final String[] mArgs;

        public MethodAndArgsCaller(Method method, String[] args) {
            mMethod = method;
            mArgs = args;
        }

        public void run() {
            try {
                mMethod.invoke(null, new Object[] { mArgs });
            } catch (IllegalAccessException ex) {
                throw new RuntimeException(ex);
            } catch (InvocationTargetException ex) {
                Throwable cause = ex.getCause();
                if (cause instanceof RuntimeException) {
                    throw (RuntimeException) cause;
                } else if (cause instanceof Error) {
                    throw (Error) cause;
                }
                throw new RuntimeException(ex);
            }
        }
    }
```

结合`ZygoteInit.main`，最终，SystemServer创建的流程以及理清楚了，后面有时间绘制一个时序图啥的。更加清晰。

到这里，ZygoteInit就孵化处一个完全体的SystemServer进程。

这里涉及到很多linux进程创建以及Java虚拟机和JNI相关的知识，只是粗略的看了一遍，并不清楚其中的原理。有时间需要系统的梳理这些知识点。





### SystemServer的启动 -- Java

和C++程序差不多，java程序也是从main函数开始。直接看看SystemServer的main函数。

```java
public static void main(String[] args) {
	new SystemServer().run();
}

private void run() {
	try {
		//设置默认时区及语言

		//[1] The system server should never make non-oneway calls
		Binder.setWarnOnBlocking(true);

		// Mmmmmm... more memory!
		VMRuntime.getRuntime().clearGrowthLimit();

		// The system server has to run all of the time, so it needs to be
		// as efficient as possible with its memory usage.
		VMRuntime.getRuntime().setTargetHeapUtilization(0.8f);

		// Some devices rely on runtime fingerprint generation, so make sure
		// we've defined it before booting further.
		Build.ensureFingerprintProperty();

		// Within the system server, it is an error to access Environment paths without
		// explicitly specifying a user.
		Environment.setUserRequired(true);

		// Within the system server, any incoming Bundles should be defused
		// to avoid throwing BadParcelableException.
		BaseBundle.setShouldDefuse(true);

		// Ensure binder calls into the system always run at foreground priority.
		BinderInternal.disableBackgroundScheduling(true);

		// Increase the number of binder threads in system_server
		BinderInternal.setMaxThreads(sMaxBinderThreads);

		// Prepare the main looper thread (this thread).
		android.os.Process.setThreadPriority(
				android.os.Process.THREAD_PRIORITY_FOREGROUND);
		android.os.Process.setCanSelfBackground(false);
		Looper.prepareMainLooper();

		// Initialize native services.
		System.loadLibrary("android_servers");

		// Check whether we failed to shut down last time we tried.
		// This call may not return.
		performPendingShutdown();

		// Initialize the system context.
		createSystemContext();

		// Create the system service manager.
		mSystemServiceManager = new SystemServiceManager(mSystemContext);
		mSystemServiceManager.setRuntimeRestarted(mRuntimeRestart);
		LocalServices.addService(SystemServiceManager.class, mSystemServiceManager);
		// Prepare the thread pool for init tasks that can be parallelized
		SystemServerInitThreadPool.get();
	} finally {
		traceEnd();  // InitBeforeStartServices
	}

    //启动其他服务
	startBootstrapServices();
	startCoreServices();
	startOtherServices();
	SystemServerInitThreadPool.shutdown();
	
	Looper.loop();
	throw new RuntimeException("Main thread loop unexpectedly exited");
}
```

> Java端的systemServer比较简单，但是涉及到了一些Android上层的东西，目前还不熟悉，在分析到这些东西的时候，在结合SystemServer来理解。

SystemServer进入java环境后，主要工作就是开启系统服务，有些服务通过init启动，并注册到service manager，SystemServer就通过binder获取其handler，并注册到自身，还有一个服务就是通过SystemServer注册到service manger的。

在完成service启动和注册后，SystemServer就进入了loop中，处理消息。

```java
//加载android_servers.so库，该库包含的源码在frameworks/base/services/目录下
System.loadLibrary("android_servers");

//检查上一次关机是否成功，不成功就直接关机
performPendingShutdown();

//这个函数，后面在分析 该过程会创建对象有ActivityThread，Instrumentation, ContextImpl，LoadedApk，Application。
createSystemContext();
```



### 总结

1. 对于SystemServer的main函数，创建了一个Looper对象，并进入了loop，那么谁会给SystemServer 发送消息呢？

2. 为什么SystemServer进程与Zygote进程通讯采用Socket而不是Binder？



## 附录

我只是简单的走了一遍代码流程，还有很多细节，目前的能力还不足以看懂。附录是大神网友的博客地址。

1. [SystemServer 上](http://gityuan.com/2016/02/14/android-system-server/)
2. [SytstemServer 下



## SystemServer的作用

