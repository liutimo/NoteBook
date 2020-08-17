---
title: Android系统内存占用裁剪		
date: 2020-08-17 09:48:22
tags:  
	- Android
	- 内存
categories: Android
---



## 使用`dumpsys meminfo`简单的查看内存信息



PSS(Proportional Set Size): 实际使用的物理内存，包含共享库（一个共享库占用Xkb， 有N个进程使用，每个进程算 X/Nkb内存到PSS中）。

OOM： Out Of Memory

Total PSS by OOM adjustment： 系统为程序OOM排序的结果， 当出现OOM异常时，按照这个表单从下至上的杀掉进程释放内存

Total PSS by category: 所有进程按照类别使用的实际物理内存。



这是系统初次开机时内存占用情况

```shell
rk3126c:/ $ dumpsys  meminfo                                                   
Applications Memory Usage (in Kilobytes):
Uptime: 605996 Realtime: 605996

Total PSS by process:
     64,210K: com.android.systemui (pid 577)
     52,995K: system (pid 364)
     18,266K: zygote (pid 180)
     16,082K: com.android.launcher3 (pid 1029 / activities)
     13,762K: com.android.phone (pid 570)
     12,201K: com.display.focsignservice:remoteConfig (pid 1091)
     12,040K: com.display.focsignservice:communicate (pid 1083)
     10,775K: com.android.inputmethod.latin (pid 623)
     10,585K: com.hikvision.dmb.service:remote (pid 689)
      8,993K: android.process.media (pid 1136)
      8,784K: com.display.focsignservice (pid 1177)
      7,048K: cameraserver (pid 185)
      7,008K: com.hikvision.localupdate (pid 1348)
      6,963K: com.hikvision.sadp:sadpservice (pid 1071)
      6,634K: com.display.appmanager:AppManager (pid 1106)
      6,518K: com.android.settings (pid 1384)
      6,179K: com.android.providers.calendar (pid 1415)
      5,652K: com.android.printspooler (pid 1184)
      4,144K: android.ext.services (pid 931)
      4,120K: mediaserver (pid 193)
      4,038K: com.android.smspush (pid 1015)
      3,280K: audioserver (pid 184)
      2,945K: mediadrmserver (pid 190)
      2,803K: surfaceflinger (pid 176)
      2,128K: media.extractor (pid 191)
      1,628K: media.codec (pid 189)
      1,512K: logd (pid 153)
      1,381K: drmserver (pid 186)
      1,205K: vold (pid 164)
      1,165K: sdcard (pid 650)
      1,161K: sdcard (pid 835)
      1,054K: netd (pid 194)
        919K: /init (pid 1)
        876K: keystore (pid 188)
        810K: app_cmd (pid 182)
        672K: installd (pid 187)
        661K: gatekeeperd (pid 195)
        651K: lmkd (pid 174)
        544K: ueventd (pid 129)
        544K: sh (pid 177)
        517K: servicemanager (pid 175)
        462K: dumpsys (pid 1478)
        450K: perfprofd (pid 196)
        392K: healthd (pid 173)
        282K: debuggerd (pid 163)
        237K: debuggerd:signaller (pid 165)

Total PSS by OOM adjustment:
     57,713K: Native
         18,266K: zygote (pid 180)
          7,048K: cameraserver (pid 185)
          4,120K: mediaserver (pid 193)
          3,280K: audioserver (pid 184)
          2,945K: mediadrmserver (pid 190)
          2,803K: surfaceflinger (pid 176)
          2,128K: media.extractor (pid 191)
          1,628K: media.codec (pid 189)
          1,512K: logd (pid 153)
          1,381K: drmserver (pid 186)
          1,205K: vold (pid 164)
          1,165K: sdcard (pid 650)
          1,161K: sdcard (pid 835)
          1,054K: netd (pid 194)
            919K: /init (pid 1)
            876K: keystore (pid 188)
            810K: app_cmd (pid 182)
            672K: installd (pid 187)
            661K: gatekeeperd (pid 195)
            651K: lmkd (pid 174)
            544K: ueventd (pid 129)
            544K: sh (pid 177)
            517K: servicemanager (pid 175)
            462K: dumpsys (pid 1478)
            450K: perfprofd (pid 196)
            392K: healthd (pid 173)
            282K: debuggerd (pid 163)
            237K: debuggerd:signaller (pid 165)
     52,995K: System
         52,995K: system (pid 364)
     77,972K: Persistent
         64,210K: com.android.systemui (pid 577)
         13,762K: com.android.phone (pid 570)
     16,082K: Foreground
         16,082K: com.android.launcher3 (pid 1029 / activities)
      8,182K: Visible
          4,144K: android.ext.services (pid 931)
          4,038K: com.android.smspush (pid 1015)
     10,775K: Perceptible
         10,775K: com.android.inputmethod.latin (pid 623)
     31,249K: A Services
         12,201K: com.display.focsignservice:remoteConfig (pid 1091)
         12,040K: com.display.focsignservice:communicate (pid 1083)
          7,008K: com.hikvision.localupdate (pid 1348)
     32,966K: B Services
         10,585K: com.hikvision.dmb.service:remote (pid 689)
          8,784K: com.display.focsignservice (pid 1177)
          6,963K: com.hikvision.sadp:sadpservice (pid 1071)
          6,634K: com.display.appmanager:AppManager (pid 1106)
     27,342K: Cached
          8,993K: android.process.media (pid 1136)
          6,518K: com.android.settings (pid 1384)
          6,179K: com.android.providers.calendar (pid 1415)
          5,652K: com.android.printspooler (pid 1184)

Total PSS by category:
     79,988K: Dalvik
     41,540K: .dex mmap
     40,657K: .so mmap
     35,234K: .oat mmap
     34,489K: Native
     27,504K: .art mmap
     17,756K: .apk mmap
     12,997K: Unknown
      9,897K: Dalvik Other
      6,515K: Other mmap
      6,012K: Stack
      1,828K: .ttf mmap
        499K: Other dev
        192K: Ashmem
        164K: .jar mmap
          4K: Cursor
          0K: Gfx dev
          0K: EGL mtrack
          0K: GL mtrack
          0K: Other mtrack

Total RAM: 1,022,936K (status normal)
 Free RAM:   482,858K (   27,342K cached pss +   383,836K cached kernel +    71,680K free)
 Used RAM:   354,970K (  287,934K used pss +    67,036K kernel)
 Lost RAM:   185,104K
     ZRAM:         4K physical used for         0K in swap (  520,908K total swap)
   Tuning: 80 (large 384), oom   184,320K, restore limit    61,440K (high-end-gfx)

```



