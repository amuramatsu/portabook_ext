# Linux kernel module for Portabook

This kernel module supports:

  * Backlight controll
  * Battery information
  * AC adapter information

at KINGJIM Portabook XMC10.

This module is tested under Ubuntu 16.04 LTS (linux 4.4 kernel).

## CAUTION

This module controll Power Management IC directly at backlight
control.  I have tested this module enough, but there is a
possibility that this module burn your Portabook.

## INSTALL

```
make
sudo make install
depmod -a
```

If you need only backlight controll or battery information module, modify below two line,

* CONFIG_PORTABOOK_EXT_BACKLIGHT
* CONFIG_PORTABOOK_EXT_BATTERY

at Makefile before make.

## USAGE

Linux cannot load this module automatically.  You need `modprobe portabook_ext` at every boot, or add `modprobe portabook_ext`
to start-up script such as /etc/rc.local.

# ポータブック用のLinux kernel module

このカーネルモジュールは、KINGJIMのポータブックXMC10で、

  * バックライトコントロール
  * 電池残量の取得
  * AC接続の有無の取得

を出来るようにします。

このカーネルモジュールは、Ubuntu 16.04 LTS（Linux 4.4カーネル）で
テストしています。

## 注意

このモジュール、特にバックライトコントロールは、ポータブックの電源
管理ICを直接コントロールします。十分テストしたつもりではありますが、
電源管理ICのコントロールをミスしていて、ポータブックが燃えてしまう
可能性は捨てきれません。もし、火災などが起こっても、当方は責任を持
てませんのでご了承ください。

## インストール方法

```
make
sudo make install
depmod -a
```

でインストール可能です。
もし、バックライトコントロールだけ、もしくは電池・電源状態取得だけ使いたいときは、Makefile の

* CONFIG_PORTABOOK_EXT_BACKLIGHT
* CONFIG_PORTABOOK_EXT_BATTERY

行を変更してmakeしてください。

## 使用方法

このモジュールは自動で読み込まれません。起動毎に、 `modprobe portabook_ext` を実行するか、/etc/rc.local などの起動スクリプトに
`modprobe portabook_ext` を追加してください。
