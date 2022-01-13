# UOS 生物认证服务接口适配文档

**API 可能会发生变化。**
该项目目前代码中的一些错误处理流程可能与此文档的表述不一致，请以此文档为准。

## 概述

UOS 的生物认证功能需要适配方在 D-Bus 上提供一个服务，负责认证的功能模块会通过 D-Bus 调用来录入、删除、认证生物特征。

D-Bus 是一种在 Linux 桌面操作系统中广泛使用的进程间通信机制。开发 UOS 生物认证服务之前需要开发人员对 D-Bus 以及如何制作 deb 软件包有一定认识，交付 UOS 生物认证服务时，需要以 deb 包的形式提供程序。

UOS 并不关心实际上的生物特征数据内容，服务程序计算得出特征之后请自行存储在磁盘上或者其他设备上，并保证这些数据不可以被非 root 用户修改。并且可以在 deb 软件包被卸载时自动地被包管理器移除。

以下列出需要适配方提供的 D-Bus 方法、属性和信号，以及一些必要的配置文件：

## 属性

* **List `Array of String`** 只读 目前已经成功保存的生物特征列表

  每条生物特征都有一个自己的 ID，是一个由调用者生成的 Version 4 UUID, 形如 `c9850890-7453-4f2b-9b78-481d95927205`, 仅由 ASCII 字符组成的、长度为 36 的字符串。该 ID 在录入新特征时作为参数传入，见 EnrollStart 的详细描述。

* **Claim `Boolean`** 只读 目前该服务是否可用

  `False` 表示该服务正在忙，`True` 表示该服务还可以进行新的认证或录入。

  如果该服务能够同时操作多个摄像头，那么仅应该在找不到其他摄像头来开启新的认证、录入时将这一属性置为 False

* **CharaType `Int32`** 只读 该服务的对应的生物特征类型

  0 表示该服务由于设备不存在等原因不可用
  4 表示该服务可以进行人脸特征的识别和认证

## 方法

* **Delete(`characteristic String`)** 

  删除一条生物特征，其 ID 为 characteristic，仅当该生物特征不存在时报错。
  移除一条 **正在被录入的** 生物特征 **不应该** 终止该生物特征的录入过程，并且 Delete 方法应该在这种情况下返回一个 `org.freedesktop.DBus.Error.FileNotFound` 错误。
  移除一条 **正在被验证的** 生物特征 **不应该** 导致该验证过程出错，并且 Delete 方法在这种情况下返回一个 `org.freedesktop.DBus.Error.InvalidArgs` 错误。

* **EnrollStart(`characteristic String`, `type Int32`, `action String`) -> (`File Descriptor`)** 开始录入一条新的生物特征

  开始录入一条新的生物特征，特征的 ID 为 characteristic，该录入行为的 ID 为 action。action 和 characteristic 的格式相同，是一个 Version 4 UUID，用来标志当前操作。
  type 为录入的生物特征类型，当该服务不能处理此类特征时返回一个 `org.freedesktop.DBus.Error.NotSupported` 错误。
  如果成功开启摄像头，则服务需要返回一个本地套接字，用于传输摄像头获得的图像。传输图像的具体方式在图像传输一节中说明。

  该方法返回错误时，不应该发生文件描述符泄漏。
  以下场景中，该方法必须返回一个错误，并且认为 “characteristic” 的录入行为 “action” 还没有开始。

  1. characteristic 已经成功录入/正在录入的生物特征重名。`org.freedesktop.DBus.Error.FileExists`
  2. action 与正在进行的其他操作重名。`org.freedesktop.DBus.Error.InvalidArgs`
  3. 开启摄像头失败。`org.freedesktop.DBus.Error.IOError`
  4. 创建文件描述符失败。`org.freedesktop.DBus.Error.IOError`

  开始录入特征 characteristic 之后，该方法正常返回，并且开始通过 EnrollStatus 信号返回录入过程中的状态。如果在开始录入之后出现错误，则通过信号来将错误报告给调用者。

* **EnrollStop(`aciton String`)**

  停止一个录入操作。成功录入一个特征并保存后，操作 “action” 并不被视为已经结束，直到该方法被调用。
  此时应该关闭摄像头，关闭打开的本地套接字，释放资源，从内存卸载使用的机器学习模型等。
  找不到 action 相对应的操作时，该方法返回一个 `org.freedesktop.DBus.Error.InvalidArgs` 错误。

* **VerifyStart(`Array of String characteristics`, `String action`) -> (`File Descriptor`)**

  开始验证生物特征。characteristics 是需要对比的生物特征的列表，action 是该验证操作的 ID。
  目前验证时不需要返回一个文件描述符。

  以下场景中，该方法必须返回一个错误，并且认为 “characteristics” 的验证行为 “action” 还没有开始。

  1. characteristic 不存在。`org.freedesktop.DBus.Error.InvalidArgs`
  2. action 与正在进行的其他操作重名。`org.freedesktop.DBus.Error.InvalidArgs`
  3. 开启摄像头失败。`org.freedesktop.DBus.Error.IOError`

  开始验证特征后，该方法正常返回，并且开始通过 VerifyStatus 信号返回认证过程中的状态。如果开始认证之后出现出错误，则通过信号来将错误报告给调用者。

* **VerifyStop(`action String`)** 

  停止验证操作。action 为要停止的验证操作的 ID。
  此时应该关闭摄像头，关闭打开的本地套接字，释放资源，从内存卸载使用的机器学习模型等。
  找不到 action 相对应的操作时，该方法返回一个 `org.freedesktop.DBus.Error.InvalidArgs` 错误。

## 信号

* **EnrollStatus(`action String`, `code Int32`, `json String`)**

  录入过程中传递状态信息用的信号。
  action 指明该信号对应的操作，code 为录入过程中的状态码，json 暂不使用。
  状态码定义如下：

  ```cpp
  enum EnrollStatus{
      FaceEnrollSuccess,
      FaceEnrollNotRealHuman, // 活体检测失败
      FaceEnrollFaceNotCenter,
      FaceEnrollFaceTooBig,
      FaceEnrollFaceTooSmall,
      FaceEnrollNoFace,
      FaceEnrollTooManyFace,
      FaceEnrollFaceNotClear,
      FaceEnrollBrightness,
      FaceEnrollFaceCovered,
      FaceEnrollCancel,
      FaceEnrollError, // 认证失败
      FaceEnrollException // 发生错误
  };
  ```

  当活体检测失败/检测到多人时，认证过程并不失败，只汇报一下状态。
  如果服务需要眨眼、张嘴等动作来进行活体检测，请联系我们更新状态码的定义。

* **VerifyStatus(`action String`, `code Int32`, `json Stirng`)**

  验证过程中使用的信号
  ```cpp
  enum VerifyStatus{
    FaceVerifySuccess,
    FaceVerifyNotRealHuman,
    FaceVerifyFaceNotCenter,
    FaceVerifyFaceTooBig,
    FaceVerifyFaceTooSmall,
    FaceVerifyNoFace,
    FaceVerifyTooManyFace,
    FaceVerifyFaceNotClear,
    FaceVerifyBrightness,
    FaceVerifyFaceCovered,
    FaceVerifyCancel,
    FaceVerifyError,
    FaceVerifyException
  };
  ```

  暂时与录入过程保持一致。

## 配置文件

以下配置文件以生物认证服务工作在：
```
name: com.example.Biometric
path: /Face
interface: com.example.Face
```
为例。

* `/usr/share/dbus-1/system.d/com.example.Biometric.conf`

    dbus 权限配置文件，是一个 XML：
    ```xml
    <?xml version="1.0" encoding="UTF-8"?> <!-- -*- XML -*- -->
    
    <!DOCTYPE busconfig PUBLIC
     "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
     "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
    <busconfig>
      <!-- Only root can own the service -->
      <policy user="root">
        <allow own="com.deepin.face"/>
      </policy>
    
      <!-- Allow anyone to invoke methods on the interfaces -->
      <policy context="default">
        <allow send_destination="com.deepin.face"/>
    
      </policy>
      <policy user="root">
        <allow own="com.deepin.face"/>
        <allow send_destination="com.deepin.face" />
      </policy>
    </busconfig>

* `/usr/share/dbus-1/system-services/com.example.Biometric.service`

    dbus 服务配置文件
    ```
    [D-BUS Service]
    Name=com.exmaple.Biometric
    User=root
    Exec=/path/to/your/service
    SystemdService=com.example.Biometric.service
    ```

* `/usr/lib/systemd/system/com.example.Biometric.service`

    systemd 服务配置文件
    ```
    [Unit]
    Description=Add your description here.

    [Service]
    User=root
    Type=dbus
    BusName=com.exmaple.Biometric
    ExecStart=/path/to/your/service
    ```

* `/usr/share/deepin-authentication/interfaces/com-example-Biometric.json`

    ```json
    {
      "service": "com.example.Biometric",
      "path": "/Face",
      "interface": "com.exmaple.Face",
      "type": 4
    }
    ```
## 图像传输

创建好 **非阻塞** 的本地套接字对后，将其中一个套接字通过 dbus 发送给调用者，向自己持有的另一个套接字中写入两个 int32

第一个 int32 固定为 0，表示以逐帧传输的方式传输数据；
第二个 int32 为图像的编码方式，目前仅支持 RGB888，此位固定为 0；

之后每获得一帧完整的图像，就先从套接字中读取一个字节的消息，如果读取失败就跳过该帧不发送数据，视为客户端在忙。
如果读取成功，则向套接字中写入该帧图像实际数据的字节数 size。

size 为 int32 类型。接下来写入 size 个字节，表示图像数据本身。
如果由于套接字缓冲区大小不足而导致以上数据写入失败，请重试，如果发现套接字已经关闭，则不再写入数据。

建议参考该项目中 sendCapture 函数的实现。

## 自动退出

当服务目前没有正在进行的操作，并且没有收到任何调用超过 15 秒钟，则该服务应该自动退出，退出时需要注销 dbus 名称。

## 数据存储

建议将数据存储在system dbus上的com.deepin.daemon.Uadp服务上。
