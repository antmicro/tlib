#include "vec_common.h"

/*
 * Data for expanding active predicate bits to bytes, for byte elements.
 *
 *  for (i = 0; i < 256; ++i) {
 *      unsigned long m = 0;
 *      for (j = 0; j < 8; j++) {
 *          if ((i >> j) & 1) {
 *              m |= 0xfful << (j << 3);
 *          }
 *      }
 *      printf("0x%016lx,\n", m);
 *  }
 */
const uint64_t expand_pred_b_data[256] = {
    0x0000000000000000, 0x00000000000000ff, 0x000000000000ff00, 0x000000000000ffff, 0x0000000000ff0000, 0x0000000000ff00ff,
    0x0000000000ffff00, 0x0000000000ffffff, 0x00000000ff000000, 0x00000000ff0000ff, 0x00000000ff00ff00, 0x00000000ff00ffff,
    0x00000000ffff0000, 0x00000000ffff00ff, 0x00000000ffffff00, 0x00000000ffffffff, 0x000000ff00000000, 0x000000ff000000ff,
    0x000000ff0000ff00, 0x000000ff0000ffff, 0x000000ff00ff0000, 0x000000ff00ff00ff, 0x000000ff00ffff00, 0x000000ff00ffffff,
    0x000000ffff000000, 0x000000ffff0000ff, 0x000000ffff00ff00, 0x000000ffff00ffff, 0x000000ffffff0000, 0x000000ffffff00ff,
    0x000000ffffffff00, 0x000000ffffffffff, 0x0000ff0000000000, 0x0000ff00000000ff, 0x0000ff000000ff00, 0x0000ff000000ffff,
    0x0000ff0000ff0000, 0x0000ff0000ff00ff, 0x0000ff0000ffff00, 0x0000ff0000ffffff, 0x0000ff00ff000000, 0x0000ff00ff0000ff,
    0x0000ff00ff00ff00, 0x0000ff00ff00ffff, 0x0000ff00ffff0000, 0x0000ff00ffff00ff, 0x0000ff00ffffff00, 0x0000ff00ffffffff,
    0x0000ffff00000000, 0x0000ffff000000ff, 0x0000ffff0000ff00, 0x0000ffff0000ffff, 0x0000ffff00ff0000, 0x0000ffff00ff00ff,
    0x0000ffff00ffff00, 0x0000ffff00ffffff, 0x0000ffffff000000, 0x0000ffffff0000ff, 0x0000ffffff00ff00, 0x0000ffffff00ffff,
    0x0000ffffffff0000, 0x0000ffffffff00ff, 0x0000ffffffffff00, 0x0000ffffffffffff, 0x00ff000000000000, 0x00ff0000000000ff,
    0x00ff00000000ff00, 0x00ff00000000ffff, 0x00ff000000ff0000, 0x00ff000000ff00ff, 0x00ff000000ffff00, 0x00ff000000ffffff,
    0x00ff0000ff000000, 0x00ff0000ff0000ff, 0x00ff0000ff00ff00, 0x00ff0000ff00ffff, 0x00ff0000ffff0000, 0x00ff0000ffff00ff,
    0x00ff0000ffffff00, 0x00ff0000ffffffff, 0x00ff00ff00000000, 0x00ff00ff000000ff, 0x00ff00ff0000ff00, 0x00ff00ff0000ffff,
    0x00ff00ff00ff0000, 0x00ff00ff00ff00ff, 0x00ff00ff00ffff00, 0x00ff00ff00ffffff, 0x00ff00ffff000000, 0x00ff00ffff0000ff,
    0x00ff00ffff00ff00, 0x00ff00ffff00ffff, 0x00ff00ffffff0000, 0x00ff00ffffff00ff, 0x00ff00ffffffff00, 0x00ff00ffffffffff,
    0x00ffff0000000000, 0x00ffff00000000ff, 0x00ffff000000ff00, 0x00ffff000000ffff, 0x00ffff0000ff0000, 0x00ffff0000ff00ff,
    0x00ffff0000ffff00, 0x00ffff0000ffffff, 0x00ffff00ff000000, 0x00ffff00ff0000ff, 0x00ffff00ff00ff00, 0x00ffff00ff00ffff,
    0x00ffff00ffff0000, 0x00ffff00ffff00ff, 0x00ffff00ffffff00, 0x00ffff00ffffffff, 0x00ffffff00000000, 0x00ffffff000000ff,
    0x00ffffff0000ff00, 0x00ffffff0000ffff, 0x00ffffff00ff0000, 0x00ffffff00ff00ff, 0x00ffffff00ffff00, 0x00ffffff00ffffff,
    0x00ffffffff000000, 0x00ffffffff0000ff, 0x00ffffffff00ff00, 0x00ffffffff00ffff, 0x00ffffffffff0000, 0x00ffffffffff00ff,
    0x00ffffffffffff00, 0x00ffffffffffffff, 0xff00000000000000, 0xff000000000000ff, 0xff0000000000ff00, 0xff0000000000ffff,
    0xff00000000ff0000, 0xff00000000ff00ff, 0xff00000000ffff00, 0xff00000000ffffff, 0xff000000ff000000, 0xff000000ff0000ff,
    0xff000000ff00ff00, 0xff000000ff00ffff, 0xff000000ffff0000, 0xff000000ffff00ff, 0xff000000ffffff00, 0xff000000ffffffff,
    0xff0000ff00000000, 0xff0000ff000000ff, 0xff0000ff0000ff00, 0xff0000ff0000ffff, 0xff0000ff00ff0000, 0xff0000ff00ff00ff,
    0xff0000ff00ffff00, 0xff0000ff00ffffff, 0xff0000ffff000000, 0xff0000ffff0000ff, 0xff0000ffff00ff00, 0xff0000ffff00ffff,
    0xff0000ffffff0000, 0xff0000ffffff00ff, 0xff0000ffffffff00, 0xff0000ffffffffff, 0xff00ff0000000000, 0xff00ff00000000ff,
    0xff00ff000000ff00, 0xff00ff000000ffff, 0xff00ff0000ff0000, 0xff00ff0000ff00ff, 0xff00ff0000ffff00, 0xff00ff0000ffffff,
    0xff00ff00ff000000, 0xff00ff00ff0000ff, 0xff00ff00ff00ff00, 0xff00ff00ff00ffff, 0xff00ff00ffff0000, 0xff00ff00ffff00ff,
    0xff00ff00ffffff00, 0xff00ff00ffffffff, 0xff00ffff00000000, 0xff00ffff000000ff, 0xff00ffff0000ff00, 0xff00ffff0000ffff,
    0xff00ffff00ff0000, 0xff00ffff00ff00ff, 0xff00ffff00ffff00, 0xff00ffff00ffffff, 0xff00ffffff000000, 0xff00ffffff0000ff,
    0xff00ffffff00ff00, 0xff00ffffff00ffff, 0xff00ffffffff0000, 0xff00ffffffff00ff, 0xff00ffffffffff00, 0xff00ffffffffffff,
    0xffff000000000000, 0xffff0000000000ff, 0xffff00000000ff00, 0xffff00000000ffff, 0xffff000000ff0000, 0xffff000000ff00ff,
    0xffff000000ffff00, 0xffff000000ffffff, 0xffff0000ff000000, 0xffff0000ff0000ff, 0xffff0000ff00ff00, 0xffff0000ff00ffff,
    0xffff0000ffff0000, 0xffff0000ffff00ff, 0xffff0000ffffff00, 0xffff0000ffffffff, 0xffff00ff00000000, 0xffff00ff000000ff,
    0xffff00ff0000ff00, 0xffff00ff0000ffff, 0xffff00ff00ff0000, 0xffff00ff00ff00ff, 0xffff00ff00ffff00, 0xffff00ff00ffffff,
    0xffff00ffff000000, 0xffff00ffff0000ff, 0xffff00ffff00ff00, 0xffff00ffff00ffff, 0xffff00ffffff0000, 0xffff00ffffff00ff,
    0xffff00ffffffff00, 0xffff00ffffffffff, 0xffffff0000000000, 0xffffff00000000ff, 0xffffff000000ff00, 0xffffff000000ffff,
    0xffffff0000ff0000, 0xffffff0000ff00ff, 0xffffff0000ffff00, 0xffffff0000ffffff, 0xffffff00ff000000, 0xffffff00ff0000ff,
    0xffffff00ff00ff00, 0xffffff00ff00ffff, 0xffffff00ffff0000, 0xffffff00ffff00ff, 0xffffff00ffffff00, 0xffffff00ffffffff,
    0xffffffff00000000, 0xffffffff000000ff, 0xffffffff0000ff00, 0xffffffff0000ffff, 0xffffffff00ff0000, 0xffffffff00ff00ff,
    0xffffffff00ffff00, 0xffffffff00ffffff, 0xffffffffff000000, 0xffffffffff0000ff, 0xffffffffff00ff00, 0xffffffffff00ffff,
    0xffffffffffff0000, 0xffffffffffff00ff, 0xffffffffffffff00, 0xffffffffffffffff,
};

/*
 * Similarly for half-word elements.
 *  for (i = 0; i < 256; ++i) {
 *      unsigned long m = 0;
 *      if (i & 0xaa) {
 *          continue;
 *      }
 *      for (j = 0; j < 8; j += 2) {
 *          if ((i >> j) & 1) {
 *              m |= 0xfffful << (j << 3);
 *          }
 *      }
 *      printf("[0x%x] = 0x%016lx,\n", i, m);
 *  }
 */
const uint64_t expand_pred_h_data[0x55 + 1] = {
    [0x01] = 0x000000000000ffff, [0x04] = 0x00000000ffff0000, [0x05] = 0x00000000ffffffff, [0x10] = 0x0000ffff00000000,
    [0x11] = 0x0000ffff0000ffff, [0x14] = 0x0000ffffffff0000, [0x15] = 0x0000ffffffffffff, [0x40] = 0xffff000000000000,
    [0x41] = 0xffff00000000ffff, [0x44] = 0xffff0000ffff0000, [0x45] = 0xffff0000ffffffff, [0x50] = 0xffffffff00000000,
    [0x51] = 0xffffffff0000ffff, [0x54] = 0xffffffffffff0000, [0x55] = 0xffffffffffffffff,
};
