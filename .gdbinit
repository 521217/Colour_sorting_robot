set confirm off
target remote localhost:3333
file main.elf
load
define fn
p/x tx
c
p/x rx
c
end
