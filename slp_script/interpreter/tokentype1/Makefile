all: tokentype1 tokentype2

tokentype1: tokentype1.cpp
	riscv64-unknown-elf-g++ -std=c++17 -Os -nostartfiles -nostdlib -nodefaultlibs -ffunction-sections -fdata-sections -fno-rtti -fno-exceptions -Wl,--gc-sections -o $@ $<
	riscv64-unknown-elf-strip -s -R .comment -R gnu.version -R .riscv.attributes --strip-unneeded $@
	#riscv64-unknown-elf-g++ -std=c++17 -Os -nostartfiles -nostdlib -nodefaultlibs -ffunction-sections -fdata-sections -fno-exceptions -Wl,--gc-sections -o $@ $<
	#riscv64-unknown-elf-strip -s -R .comment -R gnu.version -R .riscv.attributes --strip-unneeded $@


tokentype2: tokentype2.cpp
	riscv64-unknown-elf-g++ -std=c++17 -Os -ffunction-sections -fdata-sections -fno-exceptions -Wl,--gc-sections -o $@ $<
	riscv64-unknown-elf-strip -s -R .comment -R gnu.version -R .riscv.attributes --strip-unneeded $@
 
run: tokentype2
	rv-sim $< 1234

.PHONY: tokentype1 tokentype2 run
