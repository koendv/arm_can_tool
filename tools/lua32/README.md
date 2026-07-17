## luac for 32-bit

`luac32` is the lua bytecode compiler for 32-bit systems.

`luac32` for linux has been compiled with:
```
make PLAT=linux MYCFLAGS=-DLUA_32BITS
```

`luac32.exe` for windows has been compiled on linux with:
```
make PLAT=mingw CC=i686-w64-mingw32-gcc AR="i686-w64-mingw32-ar rcu" RANLIB=i686-w64-mingw32-ranlib MYCFLAGS=-DLUA_32BITS
```


