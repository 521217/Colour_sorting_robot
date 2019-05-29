set confirm off
target remote localhost:3333
file main.elf
load
break src/main.c:main
break src/sema/sema.c:15
break src/sema/sema.c:29
break src/sema/sema.c:35
set confirm on
