# SPDX-License-Identifier: GPL-2.0
ccflags-y += -Ofast

obj-$(CONFIG_ION) +=	ion-alloc.o
CFLAGS_ion.o = -I$(src)
ion-alloc-objs += ion.o ion-ioctl.o ion_heap.o
ion-alloc-$(CONFIG_ION_SYSTEM_HEAP) += ion_system_heap.o ion_page_pool.o
ion-alloc-$(CONFIG_ION_CARVEOUT_HEAP) += ion_carveout_heap.o
ion-alloc-$(CONFIG_ION_CHUNK_HEAP) += ion_chunk_heap.o
ion-alloc-$(CONFIG_ION_CMA_HEAP) += ion_cma_heap.o
