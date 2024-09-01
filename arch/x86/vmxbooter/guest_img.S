.section .guest_image
.globl input_data, input_data_end
input_data:
.incbin "vmlinux.bin"
input_data_end: