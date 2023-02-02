# sysinfo

## get interfaces, ip addresses and routes via rtnetlink

simple C++ Wrapper around [rtnetlink](https://man7.org/linux/man-pages/man7/rtnetlink.7.html)

supported messages:

* `RTM_NEWLINK`, `RTM_DELLINK`, `RTM_GETLINK`
* `RTM_NEWADDR`, `RTM_DELADDR`, `RTM_GETADDR`
* `RTM_NEWROUTE`, `RTM_DELROUTE`, `RTM_GETROUTE`

see  [example](example/main.cpp)


## dependencies

* [fmt](https://github.com/fmtlib/fmt)
* [boost::outcome](https://www.boost.org/doc/libs/master/libs/outcome/doc/html/index.html)
