import gc
import os
import samd

samd.Flash.flash_init()
bdev = samd.Flash()

# Try to mount the filesystem, and format the flash if it doesn't exist.
fs_type = os.VfsLfs2 if hasattr(os, "VfsLfs2") else uos.VfsLfs1

try:
    vfs = fs_type(bdev)
except:
    fs_type.mkfs(bdev)
    vfs = fs_type(bdev)
os.mount(vfs, "/")

gc.collect()
del os, vfs, gc
