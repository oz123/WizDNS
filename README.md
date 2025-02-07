WizDNS - a pluggable Domain Name Server
=======================================

WizDNS is a DNS written using GIO and LibPeas to provide
a plugin system that supports plugins in Python and Lua (also in C).

It is heavily inspired by CoreDNS.

Why another DNS?
----------------

Well...
First, why not?
Second, CoreDNS is written in Golang, and I have written a few
plugins in Golang for CoreDNS. However, I have been using Golang
on and off for almost 8 years now, hoping that it will have nice
inteplay with Python. But there is no easy way.
Third, I enjoy using CoreDNS and reading through the source code,
I have asked myself, how I can do it in C. So this another real
life C program that I want to write in order to learn the inner
working of CoreDNS and getting better at using glib\gio.

Compiling
---------

You will need:

 - gcc
 - glib
 - gio
 - libpeas

```
$ make wizdns
```
