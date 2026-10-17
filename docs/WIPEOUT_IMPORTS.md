# Import coverage: CUSA05670

809 imports left to the host after linking the game's own modules.

| Library | Imports | runtime | gpu | online | shadps4 | shadps4-stub | missing |
|---|---:|---:|---:|---:|---:|---:|---:|
| libkernel | 142 | 109 | 7 |  | 13 | 4 | 9 |
| libSceGnmDriver | 113 |  | 113 |  |  |  |  |
| libSceJson2 | 56 |  |  |  |  |  | 56 |
| libSceNet | 40 | 34 |  | 6 |  |  |  |
| libSceNpMatching2 | 34 | 17 |  | 17 |  |  |  |
| libScePosix | 31 | 20 |  |  | 10 |  | 1 |
| libSceHttp | 30 | 19 |  | 11 |  |  |  |
| libSceNpManager | 28 | 12 |  | 9 |  |  | 7 |
| libSceNpTus | 25 |  |  | 25 |  |  |  |
| libSceNpWebApi | 19 | 10 |  | 9 |  |  |  |
| libSceHmd | 18 |  |  |  | 12 | 6 |  |
| libSceNpScore | 18 | 8 |  | 10 |  |  |  |
| libSceAudio3d | 17 |  |  |  | 16 | 1 |  |
| libSceNpTrophy | 16 | 6 |  | 10 |  |  |  |
| libSceVideoOut | 16 |  | 16 |  |  |  |  |
| libSceAjm | 14 | 13 |  |  |  | 1 |  |
| libSceVrTracker | 12 |  |  |  | 8 | 4 |  |
| libSceAudioOut | 10 | 7 |  |  |  | 3 |  |
| libSceNpUtility | 10 |  |  |  |  |  | 10 |
| libSceSaveData | 10 | 7 |  |  | 2 | 1 |  |
| libSceCamera | 8 |  |  |  | 7 | 1 |  |
| libScePad | 8 | 8 |  |  |  |  |  |
| libSceMsgDialog | 7 | 4 |  |  | 2 | 1 |  |
| libSceNetCtl | 7 | 6 |  | 1 |  |  |  |
| libSceNpCommerce | 7 | 4 |  | 3 |  |  |  |
| libSceHmdSetupDialog | 6 |  |  |  | 1 | 5 |  |
| libScePlayGo | 6 | 5 |  |  | 1 |  |  |
| libScePlayGoDialog | 6 |  |  |  | 1 | 5 |  |
| libSceRtc | 6 | 5 |  |  | 1 |  |  |
| libSceSaveDataDialog | 6 | 4 |  |  | 1 | 1 |  |
| libSceAppContent | 5 | 3 |  |  | 1 | 1 |  |
| libSceErrorDialog | 5 |  |  |  | 3 | 2 |  |
| libSceGameCustomDataDialog | 5 |  |  |  |  |  | 5 |
| libSceInvitationDialog | 5 |  |  |  |  |  | 5 |
| libSceNpAuth | 5 | 1 |  | 4 |  |  |  |
| libSceNpProfileDialog | 5 | 4 |  | 1 |  |  |  |
| libSceNpSnsFacebookDialog | 5 |  |  | 1 |  |  | 4 |
| libSceSigninDialog | 5 |  |  |  |  | 5 |  |
| libSceSystemService | 5 | 4 |  |  | 1 |  |  |
| libSceUserService | 5 | 5 |  |  |  |  |  |
| libSceSsl | 4 | 2 |  | 2 |  |  |  |
| libSceSysmodule | 4 | 3 |  |  | 1 |  |  |
| libSceNetCtlForNpToolkit | 3 |  |  | 3 |  |  |  |
| libSceNpManagerForToolkit | 3 |  |  | 3 |  |  |  |
| libSceNpSns | 3 |  |  |  |  |  | 3 |
| libSceRemoteplay | 3 |  |  |  | 1 | 2 |  |
| libSceShareUtility | 3 |  |  |  |  |  | 3 |
| libSceDiscMap | 2 | 1 |  |  | 1 |  |  |
| libSceSocialScreen | 2 |  |  |  |  | 2 |  |
| libSceUserServiceForNpToolkit | 2 |  |  |  | 2 |  |  |
| libSceCommonDialog | 1 | 1 |  |  |  |  |  |
| libSceLibcInternal | 1 | 1 |  |  |  |  |  |
| libSceLibcInternalExt | 1 |  |  |  | 1 |  |  |
| libSceSystemServicePlatformPrivacy | 1 |  |  |  |  | 1 |  |
| **total** | **809** | **323** | **136** | **115** | **86** | **46** | **103** |

## Missing everywhere

### libSceGameCustomDataDialog (5)

- `sceGameCustomDataDialogGetResultA` (`TWi-wZVp5Pc`)
- `sceGameCustomDataDialogInitialize` (`xtbb-2f703A`)
- `sceGameCustomDataDialogOpenA` (`QHFJB2F0p3k`)
- `sceGameCustomDataDialogTerminate` (`HwEtHFCpU5U`)
- `sceGameCustomDataDialogUpdateStatus` (`PkdLsRA4ON0`)

### libSceInvitationDialog (5)

- `sceInvitationDialogGetResultA` (`WuuUhuKOxwQ`)
- `sceInvitationDialogInitialize` (`XvA5KS56wcs`)
- `sceInvitationDialogOpenA` (`sAxbHhAWMXM`)
- `sceInvitationDialogTerminate` (`B6HVJtDYxEE`)
- `sceInvitationDialogUpdateStatus` (`9+g9iOq+7kg`)

### libSceJson2 (56)

- `_ZN3sce4Json11Initializer10initializeEPKNS0_13InitParameterE` (`Cxwy7wHq4J0`)
- `_ZN3sce4Json11InitializerC1Ev` (`cK6bYHf-Q5E`)
- `_ZN3sce4Json11InitializerD1Ev` (`RujUxbr3haM`)
- `_ZN3sce4Json12MemAllocator11notifyErrorEimPv` (`i-XwZjw0OOY`)
- `_ZN3sce4Json12MemAllocatorC2Ev` (`-hJRce8wn1U`)
- `_ZN3sce4Json12MemAllocatorD2Ev` (`OcAgPxcq5Vk`)
- `_ZN3sce4Json5Array8iteratorD1Ev` (`9yLjn46Ypfs`)
- `_ZN3sce4Json5Array8iteratorppEv` (`w5+VCznos5E`)
- `_ZN3sce4Json5Array9push_backERKNS0_5ValueE` (`zQtLRTqceMY`)
- `_ZN3sce4Json5ArrayC1ERKS1_` (`bI5AGFMydrA`)
- `_ZN3sce4Json5ArrayC1Ev` (`JP-PtKMiI1E`)
- `_ZN3sce4Json5ArrayD1Ev` (`HJ8GpRT1aiw`)
- `_ZN3sce4Json5Value21setNullAccessCallBackEPFRKS1_NS0_9ValueTypeEPS2_PvES6_` (`Xbl-LYVFNEE`)
- `_ZN3sce4Json5Value3setERKNS0_6ObjectE` (`dFCphqnd+a4`)
- `_ZN3sce4Json5Value5clearEv` (`FIjXN2TkuTs`)
- `_ZN3sce4Json5Value9serializeERNS0_6StringE` (`R7FDWtcN6f8`)
- `_ZN3sce4Json5ValueC1ERKNS0_5ArrayE` (`iZeYfOxtMRg`)
- `_ZN3sce4Json5ValueC1ERKNS0_6ObjectE` (`3xUXnmUkXfo`)
- `_ZN3sce4Json5ValueC1ERKNS0_6StringE` (`sZIoMRGO+jk`)
- `_ZN3sce4Json5ValueC1Em` (`x4AUdbhpRB0`)
- `_ZN3sce4Json5ValueC1Ev` (`qBMjqyBn3OM`)
- `_ZN3sce4Json5ValueD1Ev` (`WTtYf+cNnXI`)
- `_ZN3sce4Json5ValueaSERKS1_` (`4zrm6VrgIAw`)
- `_ZN3sce4Json6Object8iteratorC1ERKS2_` (`JATlaeBu+lU`)
- `_ZN3sce4Json6Object8iteratorD1Ev` (`hoINmSMlYjI`)
- `_ZN3sce4Json6ObjectC1ERKS1_` (`a+W7HHlwpBs`)
- `_ZN3sce4Json6ObjectC1Ev` (`OJPTonqdg0I`)
- `_ZN3sce4Json6ObjectD1Ev` (`5JmzZt8twAo`)
- `_ZN3sce4Json6ObjectixERKNS0_6StringE` (`ERuf9y0DY84`)
- `_ZN3sce4Json6Parser5parseERNS0_5ValueEPKcm` (`S5JxQnoGF3E`)
- `_ZN3sce4Json6StringC1EPKc` (`9KUZFjI1IxA`)
- `_ZN3sce4Json6StringC1ERKS1_` (`0CAesfH963Q`)
- `_ZN3sce4Json6StringC1Ev` (`qSmqLXXCPas`)
- `_ZN3sce4Json6StringD1Ev` (`cG1VE2HMl6c`)
- `_ZN3sce4Json6StringaSERKS1_` (`cn9svYGWKDQ`)
- `_ZNK3sce4Json5Array3endEv` (`WXF2ihRF+B8`)
- `_ZNK3sce4Json5Array5beginEv` (`bcH5EnFE2xY`)
- `_ZNK3sce4Json5Array8iteratorneERKS2_` (`5AZPp99ogrc`)
- `_ZNK3sce4Json5Array8iteratorptEv` (`iAIYn4oAWvI`)
- `_ZNK3sce4Json5Value10getBooleanEv` (`zTwZdI8AZ5Y`)
- `_ZNK3sce4Json5Value10getIntegerEv` (`DIxvoy7Ngvk`)
- `_ZNK3sce4Json5Value11getUIntegerEv` (`sn4HNCtNRzY`)
- `_ZNK3sce4Json5Value7getRealEv` (`3qrge7L-AU4`)
- `_ZNK3sce4Json5Value7getTypeEv` (`SHtAad20YYM`)
- `_ZNK3sce4Json5Value8getArrayEv` (`ONT8As5R1ug`)
- `_ZNK3sce4Json5Value8getValueERKNS0_6StringE` (`MsMOdxWfbwQ`)
- `_ZNK3sce4Json5Value9getObjectEv` (`IlsmvBtMkak`)
- `_ZNK3sce4Json5Value9getStringEv` (`epJ6x2LV0kU`)
- `_ZNK3sce4Json5ValueixEPKc` (`HwDt5lD9Bfo`)
- `_ZNK3sce4Json6Object3endEv` (`ivMCitpSQNk`)
- `_ZNK3sce4Json6Object4findERKNS0_6StringE` (`ehN1Biw3tw8`)
- `_ZNK3sce4Json6Object8iteratoreqERKS2_` (`q3YO1c0RqFo`)
- `_ZNK3sce4Json6Object8iteratorneERKS2_` (`+isUKw4zud4`)
- `_ZNK3sce4Json6Object8iteratorptEv` (`EnesV4gXDtE`)
- `_ZNK3sce4Json6String4sizeEv` (`UVBkipSD2sw`)
- `_ZNK3sce4Json6String5c_strEv` (`L1KAkYWml-M`)

### libSceNpManager (7)

- `sceNpInGameMessageCreateHandle` (`s4UEa5iBJdc`)
- `sceNpInGameMessageDeleteHandle` (`+anuSx2avHQ`)
- `sceNpInGameMessageGetMemoryPoolStatistics` (`Ubv+fP58W1U`)
- `sceNpInGameMessageInitialize` (`GFhVUpRmbHE`)
- `sceNpInGameMessagePrepareA` (`IkL62FMpIpo`)
- `sceNpInGameMessageSendDataA` (`PQDFxcnqxtw`)
- `sceNpInGameMessageTerminate` (`bMG3cVmUmuk`)

### libSceNpSns (3)

- `sceNpSnsFacebookCreateRequest` (`OoTjfxl8-wI`)
- `sceNpSnsFacebookDeleteRequest` (`pTvAKV1iQkE`)
- `sceNpSnsFacebookGetAccessToken` (`VtyS8XLBqNE`)

### libSceNpSnsFacebookDialog (4)

- `sceNpSnsFacebookDialogGetResult` (`AifdcI1aKas`)
- `sceNpSnsFacebookDialogInitialize` (`tdQArULbNQk`)
- `sceNpSnsFacebookDialogOpen` (`Hi3onqHSN6M`)
- `sceNpSnsFacebookDialogTerminate` (`cdqOFyupRS4`)

### libSceNpUtility (10)

- `sceNpBandwidthTestAbort` (`kvdMF48mB3Y`)
- `sceNpBandwidthTestGetStatus` (`BYIZGKm6bO4`)
- `sceNpBandwidthTestInitStart` (`jktww3yJXnc`)
- `sceNpBandwidthTestShutdown` (`pLr1fEQS1z8`)
- `sceNpWordFilterAbortRequest` (`rAOOqDAxBIk`)
- `sceNpWordFilterCreateRequest` (`iCq5xW5KQW4`)
- `sceNpWordFilterCreateTitleCtxA` (`6p9jvljuvsw`)
- `sceNpWordFilterDeleteRequest` (`PYFS1H70bDs`)
- `sceNpWordFilterDeleteTitleCtx` (`t0P5z5yuFPA`)
- `sceNpWordFilterSanitizeComment` (`Jj4mkpFO2gE`)

### libScePosix (1)

- `fcntl` (`8nY19bKoiZk`)

### libSceShareUtility (3)

- `sceShareUtilityInitializeEx2` (`j7DlalBzHh8`)
- `sceShareUtilityOpenShareMenuDefault` (`8hZ2EEl2Tto`)
- `sceShareUtilityTerminate` (`DUWhxkyVPj4`)

### libkernel (9)

- `__elf_phdr_match_addr` (`Fjc4-n1+y2g`)
- `__progname` (`djxxOmW6-aw`)
- `__pthread_cxa_finalize` (`kbw4UHHSYy0`)
- `_is_signal_return` (`crb5j7mkk1c`)
- `sceKernelInternalMemoryGetModuleSegmentInfo` (`-YTW+qXc3CQ`)
- `sceKernelStopUnloadModule` (`QKd0qM58Qes`)
- `scePthreadCondattrDestroy` (`waPcxYiR3WA`)
- `scePthreadCondattrInit` (`m5-2bsNfv7s`)
- `sigreturn` (`mo0bFmWppIw`)


## Only a stub in shadPS4 (needs a real implementation or a checked stub)

### libSceAjm (1)

- `sceAjmMemoryUnregister` (`pIpGiaYkHkM`) — `ajm/ajm.cpp`

### libSceAppContent (1)

- `sceAppContentAddcontEnqueueDownload` (`7gxh+5QubhY`) — `app_content/app_content.cpp`

### libSceAudio3d (1)

- `sceAudio3dBedWrite` (`9tEwE0GV0qo`) — `audio3d/audio3d_openal.cpp`

### libSceAudioOut (3)

- `sceAudioOutMasteringInit` (`xX4RLegarbg`) — `audio/audioout.cpp`
- `sceAudioOutMasteringSetParam` (`4055yaUg3EY`) — `audio/audioout.cpp`
- `sceAudioOutMasteringTerm` (`RVWtUgoif5o`) — `audio/audioout.cpp`

### libSceCamera (1)

- `sceCameraSetVideoSync` (`wpeyFwJ+UEI`) — `camera/camera.cpp`

### libSceErrorDialog (2)

- `sceErrorDialogGetStatus` (`t2FvHRXzgqk`) — `ime/error_dialog.cpp`
- `sceErrorDialogUpdateStatus` (`WWiGuh9XfgQ`) — `ime/error_dialog.cpp`

### libSceHmd (6)

- `sceHmdReprojectionQueryGarlicBuffAlign` (`TkcANcGM0s8`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionQueryGarlicBuffSize` (`z0KtN1vqF2E`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionQueryOnionBuffAlign` (`IWybWbR-xvA`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionQueryOnionBuffSize` (`kLUAkN6a1e8`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionSetOutputMinColor` (`LjdLRysHU6Y`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionStop` (`vzMEkwBQciM`) — `hmd/hmd_reprojection.cpp`

### libSceHmdSetupDialog (5)

- `sceHmdSetupDialogClose` (`nmHzU4Gh0xs`) — `hmd/hmd_setup_dialog.cpp`
- `sceHmdSetupDialogInitialize` (`NB1Y2kA2jCY`) — `hmd/hmd_setup_dialog.cpp`
- `sceHmdSetupDialogOpen` (`NNgiV4T+akU`) — `hmd/hmd_setup_dialog.cpp`
- `sceHmdSetupDialogTerminate` (`+z4OJmFreZc`) — `hmd/hmd_setup_dialog.cpp`
- `sceHmdSetupDialogUpdateStatus` (`Ud7j3+RDIBg`) — `hmd/hmd_setup_dialog.cpp`

### libSceMsgDialog (1)

- `sceMsgDialogGetStatus` (`CWVW78Qc3fI`) — `system/msgdialog.cpp`

### libScePlayGoDialog (5)

- `scePlayGoDialogClose` (`fbigNQiZpm0`) — `playgo/playgo_dialog.cpp`
- `scePlayGoDialogGetStatus` (`NOAMxY2EGS0`) — `playgo/playgo_dialog.cpp`
- `scePlayGoDialogInitialize` (`fECamTJKpsM`) — `playgo/playgo_dialog.cpp`
- `scePlayGoDialogTerminate` (`okgIGdr5Iz0`) — `playgo/playgo_dialog.cpp`
- `scePlayGoDialogUpdateStatus` (`Yb60K7BST48`) — `playgo/playgo_dialog.cpp`

### libSceRemoteplay (2)

- `sceRemoteplayInitialize` (`k1SwgkMSOM8`) — `remote_play/remoteplay.cpp`
- `sceRemoteplayTerminate` (`BOwybKVa3Do`) — `remote_play/remoteplay.cpp`

### libSceSaveData (1)

- `sceSaveDataUmountWithBackup` (`VwadwBBBJ80`) — `save_data/savedata.cpp`

### libSceSaveDataDialog (1)

- `sceSaveDataDialogGetStatus` (`ERKzksauAJA`) — `save_data/dialog/savedatadialog.cpp`

### libSceSigninDialog (5)

- `sceSigninDialogGetResult` (`nqG7rqnYw1U`) — `signin_dialog/signindialog.cpp`
- `sceSigninDialogInitialize` (`mlYGfmqE3fQ`) — `signin_dialog/signindialog.cpp`
- `sceSigninDialogOpen` (`JlpJVoRWv7U`) — `signin_dialog/signindialog.cpp`
- `sceSigninDialogTerminate` (`LXlmS6PvJdU`) — `signin_dialog/signindialog.cpp`
- `sceSigninDialogUpdateStatus` (`Bw31liTFT3A`) — `signin_dialog/signindialog.cpp`

### libSceSocialScreen (2)

- `sceSocialScreenInitialize` (`pI7oFSPP65A`) — `social_screen/social_screen.cpp`
- `sceSocialScreenTerminate` (`OVNpYTRqN74`) — `social_screen/social_screen.cpp`

### libSceSystemServicePlatformPrivacy (1)

- `sceSystemServiceGetPlatformPrivacySetting` (`hvoLYhc4cq0`) — `system/systemservice.cpp`

### libSceVrTracker (4)

- `sceVrTrackerGetPlayAreaWarningInfo` (`zvyKP0Z3UvU`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerGpuWaitAndCpuProcess` (`ARhgpXvwoR0`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerRegisterDevice` (`sIh8GwcevaQ`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerUpdateMotionSensorData` (`9fvHMUbsom4`) — `vr_tracker/vr_tracker.cpp`

### libkernel (4)

- `_nanosleep` (`NhpspxdjEKU`) — `kernel/time.cpp`
- `msync` (`tZY4+SZNFhA`) — `kernel/memory.cpp`
- `sceKernelIsAddressSanitizerEnabled` (`jh+8XiK4LeE`) — `kernel/memory.cpp`
- `sceKernelMlock` (`3k6kx-zOOSQ`) — `kernel/memory.cpp`


## Implemented in shadPS4: to port

### libSceAppContent (1)

- `sceAppContentGetEntitlementKey` (`XTWR0UXvcgs`) — `app_content/app_content.cpp`

### libSceAudio3d (16)

- `sceAudio3dAudioOutOpen` (`ucEsi62soTo`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dAudioOutOutputs` (`HbxYY27lK6E`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dGetDefaultOpenParameters` (`Im+jOoa5WAI`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dInitialize` (`UmCvjSmuZIw`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dObjectReserve` (`jO2tec4dJ2M`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dObjectSetAttributes` (`4uyHN9q4ZeU`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dObjectUnreserve` (`1HXxo-+1qCw`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortAdvance` (`lw0qrdSjZt8`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortClose` (`OyVqOeVNtSk`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortFlush` (`ZOGrxWLgQzE`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortGetAttributesSupported` (`9ZA23Ia46Po`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortGetQueueLevel` (`YaaDbDwKpFM`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortOpen` (`XeDDK0xJWQA`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortPush` (`VEVhZ9qd4ZY`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dPortSetAttribute` (`Yq9bfUQ0uJg`) — `audio3d/audio3d_openal.cpp`
- `sceAudio3dTerminate` (`WW1TS2iz5yc`) — `audio3d/audio3d_openal.cpp`

### libSceCamera (7)

- `sceCameraClose` (`OMS9LlcrvBo`) — `camera/camera.cpp`
- `sceCameraGetFrameData` (`mxgMmR+1Kr0`) — `camera/camera.cpp`
- `sceCameraIsAttached` (`p6n3Npi3YY4`) — `camera/camera.cpp`
- `sceCameraOpen` (`BHn83xrF92E`) — `camera/camera.cpp`
- `sceCameraSetConfig` (`VQ+5kAqsE2Q`) — `camera/camera.cpp`
- `sceCameraStart` (`9EpRYMy7rHU`) — `camera/camera.cpp`
- `sceCameraStop` (`2G2C0nmd++M`) — `camera/camera.cpp`

### libSceDiscMap (1)

- `fJgP+wqifno` (`fJgP+wqifno`) — `disc_map/disc_map.cpp`

### libSceErrorDialog (3)

- `sceErrorDialogClose` (`ekXHb1kDBl0`) — `ime/error_dialog.cpp`
- `sceErrorDialogInitialize` (`I88KChlynSs`) — `ime/error_dialog.cpp`
- `sceErrorDialogOpen` (`M2ZF-ClLhgY`) — `ime/error_dialog.cpp`

### libSceHmd (12)

- `sceHmdClose` (`6biw1XHTSqQ`) — `hmd/hmd.cpp`
- `sceHmdGetDeviceInformation` (`thDt9upZlp8`) — `hmd/hmd.cpp`
- `sceHmdGetDeviceInformationByHandle` (`1pxQfif1rkE`) — `hmd/hmd.cpp`
- `sceHmdGetFieldOfView` (`NPQwYFqi0bs`) — `hmd/hmd.cpp`
- `sceHmdInitialize315` (`s-J66ar9g50`) — `hmd/hmd.cpp`
- `sceHmdOpen` (`d2g5Ij7EUzo`) — `hmd/hmd.cpp`
- `sceHmdReprojectionFinalize` (`ZrV5YIqD09I`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionInitialize` (`OuygGEWkins`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionSetDisplayBuffers` (`E+dPfjeQLHI`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionStart` (`dntZTJ7meIU`) — `hmd/hmd_reprojection.cpp`
- `sceHmdReprojectionUnsetDisplayBuffers` (`iGNNpDDjcwo`) — `hmd/hmd_reprojection.cpp`
- `sceHmdTerminate` (`z-RMILqP6tE`) — `hmd/hmd.cpp`

### libSceHmdSetupDialog (1)

- `sceHmdSetupDialogGetResult` (`6lVRHMV5LY0`) — `hmd/hmd_setup_dialog.cpp`

### libSceLibcInternalExt (1)

- `sceLibcHeapGetTraceInfo` (`NWtTN10cJzE`) — `kernel/kernel.cpp`

### libSceMsgDialog (2)

- `sceMsgDialogClose` (`HTrcDKlFKuM`) — `system/msgdialog.cpp`
- `sceMsgDialogGetResult` (`Lr8ovHH9l6A`) — `system/msgdialog.cpp`

### libScePlayGo (1)

- `scePlayGoTerminate` (`MPe0EeBGM-E`) — `playgo/playgo.cpp`

### libScePlayGoDialog (1)

- `scePlayGoDialogOpen` (`kHd72ukqbxw`) — `playgo/playgo_dialog.cpp`

### libScePosix (10)

- `bind` (`KuOmgKoqCdY`) — `kernel/kernel.cpp`
- `getsockname` (`RenI1lL1WFk`) — `kernel/kernel.cpp`
- `getsockopt` (`6O8EwYOgH9Y`) — `kernel/kernel.cpp`
- `inet_ntop` (`5jRCs2axtr4`) — `kernel/kernel.cpp`
- `pthread_condattr_destroy` (`dJcuQVn6-Iw`) — `kernel/threads/condvar.cpp`
- `pthread_condattr_init` (`mKoTx03HRWA`) — `kernel/threads/condvar.cpp`
- `recvfrom` (`lUk6wrGXyMw`) — `kernel/kernel.cpp`
- `sendto` (`oBr313PppNE`) — `kernel/kernel.cpp`
- `setsockopt` (`fFxGkxF2bVo`) — `kernel/kernel.cpp`
- `shutdown` (`TUuiYS2kE8s`) — `kernel/kernel.cpp`

### libSceRemoteplay (1)

- `sceRemoteplayGetConnectionStatus` (`g3PNjYKWqnQ`) — `remote_play/remoteplay.cpp`

### libSceRtc (1)

- `sceRtcParseDateTime` (`NxEI1KByvCI`) — `rtc/rtc.cpp`

### libSceSaveData (2)

- `sceSaveDataCheckBackupData` (`RQOqDbk3bSU`) — `save_data/savedata.cpp`
- `sceSaveDataRestoreBackupData` (`lU9YRFsgwSU`) — `save_data/savedata.cpp`

### libSceSaveDataDialog (1)

- `sceSaveDataDialogClose` (`fH46Lag88XY`) — `save_data/dialog/savedatadialog.cpp`

### libSceSysmodule (1)

- `sceSysmoduleLoadModuleInternalWithArg` (`hHrGoGoNf+s`) — `sysmodule/sysmodule.cpp`

### libSceSystemService (1)

- `sceSystemServiceGetDisplaySafeAreaInfo` (`1n37q1Bvc5Y`) — `system/systemservice.cpp`

### libSceUserServiceForNpToolkit (2)

- `sceUserServiceRegisterEventCallback` (`wuI7c7UNk0A`) — `system/userservice.cpp`
- `sceUserServiceUnregisterEventCallback` (`spW--yoLQ9o`) — `system/userservice.cpp`

### libSceVrTracker (8)

- `sceVrTrackerGetResult` (`76OBvrrQXUc`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerGetTime` (`XoeWzXlrnMw`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerGpuSubmit` (`TVegDMLaBB8`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerInit` (`QkRl7pART9M`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerQueryMemory` (`K7yhYrsIBPc`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerRecalibrate` (`EUCaQtXXYNI`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerTerm` (`IBv4P3q1pQ0`) — `vr_tracker/vr_tracker.cpp`
- `sceVrTrackerUnregisterDevice` (`Q8skQqEwn5c`) — `vr_tracker/vr_tracker.cpp`

### libkernel (13)

- `sceKernelAvailableDirectMemorySize` (`C0f7TJcbfac`) — `kernel/memory.cpp`
- `sceKernelConvertLocaltimeToUtc` (`0NTHN1NKONI`) — `kernel/time.cpp`
- `sceKernelConvertUtcToLocaltime` (`-o5uEDpN+oY`) — `kernel/time.cpp`
- `sceKernelCreateEventFlag` (`BpFoboUJoZU`) — `kernel/threads/event_flag.cpp`
- `sceKernelDeleteEventFlag` (`8mql9OcQnd4`) — `kernel/threads/event_flag.cpp`
- `sceKernelDlsym` (`LwG8g3niqwA`) — `kernel/process.cpp`
- `sceKernelGetModuleInfoForUnwind` (`RpQJJVKTiFM`) — `kernel/process.cpp`
- `sceKernelGetModuleInfoFromAddr` (`f7KBOafysXo`) — `kernel/process.cpp`
- `sceKernelIsNeoMode` (`WslcK1FQcGI`) — `kernel/process.cpp`
- `sceKernelLoadStartModule` (`wzvqT4UqKX8`) — `kernel/process.cpp`
- `sceKernelSetEventFlag` (`IOnSvHzqu6A`) — `kernel/threads/event_flag.cpp`
- `sceKernelUuidCreate` (`Xjoosiw+XPI`) — `kernel/kernel.cpp`
- `sceKernelWaitEventFlag` (`JTvBflhYazQ`) — `kernel/threads/event_flag.cpp`

