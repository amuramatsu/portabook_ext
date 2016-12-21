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
