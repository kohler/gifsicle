/* quantize.c - Histograms and quantization for gifsicle.
   Copyright (C) 1997-2013 Eddie Kohler, ekohler@gmail.com
   This file is part of gifsicle.

   Gifsicle is free software. It is distributed under the GNU Public License,
   version 2; you can copy, distribute, or alter it at will, as long
   as this notice is kept intact and this source code is made available. There
   is no warranty, express or implied. */

#include <config.h>
#include "gifsicle.h"
#include <assert.h>
#include <string.h>
#include <math.h>

/* kd3_color: a 3D vector, each component has 15 bits of precision */
typedef struct kd3_color {
    int a[3];
} kd3_color;

static inline void kd3_set8(kd3_color* x, int a0, int a1, int a2) {
    x->a[0] = (a0 << 7) | (a0 >> 1);
    x->a[1] = (a1 << 7) | (a1 >> 1);
    x->a[2] = (a2 << 7) | (a2 >> 1);
}

static inline unsigned kd3_distance(const kd3_color* x, const kd3_color* y) {
    return (x->a[0] - y->a[0]) * (x->a[0] - y->a[0])
        + (x->a[1] - y->a[1]) * (x->a[1] - y->a[1])
        + (x->a[2] - y->a[2]) * (x->a[2] - y->a[2]);
}


static const uint16_t srgb_gamma_array[1024] = {
    0, 2, 5, 7, 10, 12, 15, 17,
    20, 22, 25, 27, 30, 32, 35, 37,
    40, 42, 45, 47, 50, 52, 55, 57,
    59, 62, 64, 67, 69, 72, 74, 77,
    79, 82, 84, 87, 89, 92, 94, 97,
    99, 102, 104, 107, 109, 112, 115, 117,
    120, 123, 126, 129, 131, 134, 137, 140,
    143, 146, 150, 153, 156, 159, 162, 166,
    169, 173, 176, 179, 183, 187, 190, 194,
    197, 201, 205, 209, 213, 216, 220, 224,
    228, 232, 236, 241, 245, 249, 253, 257,
    262, 266, 271, 275, 280, 284, 289, 293,
    298, 303, 307, 312, 317, 322, 327, 332,
    337, 342, 347, 352, 357, 363, 368, 373,
    379, 384, 390, 395, 401, 406, 412, 418,
    423, 429, 435, 441, 447, 453, 459, 465,
    471, 477, 483, 490, 496, 502, 509, 515,
    522, 528, 535, 541, 548, 555, 561, 568,
    575, 582, 589, 596, 603, 610, 617, 625,
    632, 639, 647, 654, 661, 669, 676, 684,
    692, 699, 707, 715, 723, 731, 739, 747,
    755, 763, 771, 779, 787, 796, 804, 812,
    821, 829, 838, 847, 855, 864, 873, 882,
    890, 899, 908, 917, 926, 936, 945, 954,
    963, 973, 982, 991, 1001, 1010, 1020, 1030,
    1039, 1049, 1059, 1069, 1079, 1089, 1099, 1109,
    1119, 1129, 1139, 1150, 1160, 1170, 1181, 1191,
    1202, 1213, 1223, 1234, 1245, 1256, 1267, 1278,
    1289, 1300, 1311, 1322, 1333, 1344, 1356, 1367,
    1379, 1390, 1402, 1413, 1425, 1437, 1448, 1460,
    1472, 1484, 1496, 1508, 1520, 1533, 1545, 1557,
    1569, 1582, 1594, 1607, 1619, 1632, 1645, 1657,
    1670, 1683, 1696, 1709, 1722, 1735, 1748, 1762,
    1775, 1788, 1802, 1815, 1828, 1842, 1856, 1869,
    1883, 1897, 1911, 1925, 1939, 1953, 1967, 1981,
    1995, 2009, 2024, 2038, 2053, 2067, 2082, 2096,
    2111, 2126, 2140, 2155, 2170, 2185, 2200, 2215,
    2231, 2246, 2261, 2276, 2292, 2307, 2323, 2338,
    2354, 2370, 2386, 2401, 2417, 2433, 2449, 2465,
    2482, 2498, 2514, 2530, 2547, 2563, 2580, 2596,
    2613, 2630, 2646, 2663, 2680, 2697, 2714, 2731,
    2748, 2765, 2783, 2800, 2817, 2835, 2852, 2870,
    2887, 2905, 2923, 2941, 2959, 2977, 2995, 3013,
    3031, 3049, 3067, 3086, 3104, 3123, 3141, 3160,
    3178, 3197, 3216, 3235, 3254, 3272, 3292, 3311,
    3330, 3349, 3368, 3388, 3407, 3427, 3446, 3466,
    3485, 3505, 3525, 3545, 3565, 3585, 3605, 3625,
    3645, 3666, 3686, 3706, 3727, 3747, 3768, 3789,
    3809, 3830, 3851, 3872, 3893, 3914, 3935, 3956,
    3978, 3999, 4020, 4042, 4063, 4085, 4106, 4128,
    4150, 4172, 4194, 4216, 4238, 4260, 4282, 4304,
    4327, 4349, 4372, 4394, 4417, 4439, 4462, 4485,
    4508, 4531, 4554, 4577, 4600, 4623, 4647, 4670,
    4693, 4717, 4740, 4764, 4788, 4811, 4835, 4859,
    4883, 4907, 4931, 4955, 4980, 5004, 5028, 5053,
    5077, 5102, 5127, 5151, 5176, 5201, 5226, 5251,
    5276, 5301, 5326, 5352, 5377, 5402, 5428, 5454,
    5479, 5505, 5531, 5557, 5582, 5608, 5634, 5661,
    5687, 5713, 5739, 5766, 5792, 5819, 5845, 5872,
    5899, 5926, 5953, 5980, 6007, 6034, 6061, 6088,
    6116, 6143, 6170, 6198, 6226, 6253, 6281, 6309,
    6337, 6365, 6393, 6421, 6449, 6477, 6506, 6534,
    6563, 6591, 6620, 6649, 6677, 6706, 6735, 6764,
    6793, 6822, 6852, 6881, 6910, 6940, 6969, 6999,
    7028, 7058, 7088, 7118, 7148, 7178, 7208, 7238,
    7268, 7298, 7329, 7359, 7390, 7420, 7451, 7482,
    7513, 7544, 7575, 7606, 7637, 7668, 7699, 7730,
    7762, 7793, 7825, 7857, 7888, 7920, 7952, 7984,
    8016, 8048, 8080, 8112, 8145, 8177, 8210, 8242,
    8275, 8307, 8340, 8373, 8406, 8439, 8472, 8505,
    8538, 8572, 8605, 8638, 8672, 8706, 8739, 8773,
    8807, 8841, 8875, 8909, 8943, 8977, 9011, 9046,
    9080, 9115, 9149, 9184, 9219, 9253, 9288, 9323,
    9358, 9393, 9429, 9464, 9499, 9535, 9570, 9606,
    9641, 9677, 9713, 9749, 9785, 9821, 9857, 9893,
    9929, 9966, 10002, 10039, 10075, 10112, 10149, 10185,
    10222, 10259, 10296, 10333, 10371, 10408, 10445, 10483,
    10520, 10558, 10596, 10633, 10671, 10709, 10747, 10785,
    10823, 10861, 10900, 10938, 10977, 11015, 11054, 11092,
    11131, 11170, 11209, 11248, 11287, 11326, 11366, 11405,
    11444, 11484, 11523, 11563, 11603, 11643, 11682, 11722,
    11762, 11803, 11843, 11883, 11923, 11964, 12004, 12045,
    12086, 12126, 12167, 12208, 12249, 12290, 12331, 12373,
    12414, 12455, 12497, 12538, 12580, 12622, 12664, 12705,
    12747, 12790, 12832, 12874, 12916, 12959, 13001, 13044,
    13086, 13129, 13172, 13214, 13257, 13300, 13344, 13387,
    13430, 13473, 13517, 13560, 13604, 13648, 13691, 13735,
    13779, 13823, 13867, 13911, 13956, 14000, 14044, 14089,
    14133, 14178, 14223, 14268, 14312, 14357, 14403, 14448,
    14493, 14538, 14584, 14629, 14675, 14720, 14766, 14812,
    14858, 14904, 14950, 14996, 15042, 15088, 15135, 15181,
    15228, 15275, 15321, 15368, 15415, 15462, 15509, 15556,
    15603, 15651, 15698, 15746, 15793, 15841, 15888, 15936,
    15984, 16032, 16080, 16128, 16177, 16225, 16273, 16322,
    16370, 16419, 16468, 16517, 16565, 16614, 16664, 16713,
    16762, 16811, 16861, 16910, 16960, 17009, 17059, 17109,
    17159, 17209, 17259, 17309, 17359, 17410, 17460, 17511,
    17561, 17612, 17663, 17714, 17765, 17816, 17867, 17918,
    17969, 18021, 18072, 18124, 18175, 18227, 18279, 18331,
    18383, 18435, 18487, 18539, 18591, 18644, 18696, 18749,
    18801, 18854, 18907, 18960, 19013, 19066, 19119, 19172,
    19226, 19279, 19333, 19386, 19440, 19494, 19548, 19602,
    19656, 19710, 19764, 19818, 19873, 19927, 19982, 20036,
    20091, 20146, 20201, 20256, 20311, 20366, 20421, 20477,
    20532, 20588, 20643, 20699, 20755, 20810, 20866, 20922,
    20979, 21035, 21091, 21148, 21204, 21261, 21317, 21374,
    21431, 21488, 21545, 21602, 21659, 21716, 21774, 21831,
    21889, 21946, 22004, 22062, 22120, 22178, 22236, 22294,
    22352, 22411, 22469, 22527, 22586, 22645, 22704, 22762,
    22821, 22880, 22940, 22999, 23058, 23118, 23177, 23237,
    23296, 23356, 23416, 23476, 23536, 23596, 23656, 23716,
    23777, 23837, 23898, 23959, 24019, 24080, 24141, 24202,
    24263, 24324, 24386, 24447, 24509, 24570, 24632, 24693,
    24755, 24817, 24879, 24941, 25003, 25066, 25128, 25191,
    25253, 25316, 25379, 25441, 25504, 25567, 25630, 25694,
    25757, 25820, 25884, 25947, 26011, 26075, 26138, 26202,
    26266, 26330, 26395, 26459, 26523, 26588, 26652, 26717,
    26782, 26846, 26911, 26976, 27041, 27107, 27172, 27237,
    27303, 27368, 27434, 27500, 27565, 27631, 27697, 27764,
    27830, 27896, 27962, 28029, 28095, 28162, 28229, 28296,
    28363, 28430, 28497, 28564, 28631, 28699, 28766, 28834,
    28901, 28969, 29037, 29105, 29173, 29241, 29309, 29378,
    29446, 29515, 29583, 29652, 29721, 29790, 29859, 29928,
    29997, 30066, 30135, 30205, 30274, 30344, 30414, 30484,
    30553, 30623, 30694, 30764, 30834, 30904, 30975, 31045,
    31116, 31187, 31258, 31329, 31400, 31471, 31542, 31613,
    31685, 31756, 31828, 31899, 31971, 32043, 32115, 32187,
    32259, 32332, 32404, 32476, 32549, 32621, 32694, 32767
};

static uint16_t* gamma_array = (uint16_t*) srgb_gamma_array;

static inline void kd3_gamma_transform(kd3_color* x) {
    x->a[0] = gamma_array[x->a[0] >> 5];
    x->a[1] = gamma_array[x->a[1] >> 5];
    x->a[2] = gamma_array[x->a[2] >> 5];
}


static const uint16_t srgb_revgamma_array[1024] = {
    0, 32, 64, 96, 128, 160, 192, 224,
    256, 288, 320, 352, 384, 416, 448, 480,
    512, 545, 577, 609, 641, 673, 705, 737,
    769, 801, 833, 865, 897, 929, 961, 993,
    1025, 1057, 1089, 1121, 1153, 1185, 1217, 1249,
    1281, 1313, 1345, 1377, 1409, 1441, 1473, 1505,
    1537, 1569, 1602, 1634, 1666, 1698, 1730, 1762,
    1794, 1826, 1858, 1890, 1922, 1954, 1986, 2018,
    2050, 2082, 2114, 2146, 2178, 2210, 2242, 2274,
    2306, 2338, 2370, 2402, 2434, 2466, 2498, 2530,
    2562, 2594, 2626, 2659, 2691, 2723, 2755, 2787,
    2819, 2851, 2883, 2915, 2947, 2979, 3011, 3043,
    3075, 3107, 3139, 3171, 3203, 3235, 3267, 3299,
    3331, 3363, 3395, 3427, 3459, 3491, 3523, 3555,
    3587, 3619, 3651, 3683, 3716, 3748, 3780, 3812,
    3844, 3876, 3908, 3940, 3972, 4004, 4036, 4068,
    4100, 4132, 4164, 4196, 4228, 4260, 4292, 4324,
    4356, 4388, 4420, 4452, 4484, 4516, 4548, 4580,
    4612, 4644, 4676, 4708, 4740, 4773, 4805, 4837,
    4869, 4901, 4933, 4965, 4997, 5029, 5061, 5093,
    5125, 5157, 5189, 5221, 5253, 5285, 5317, 5349,
    5381, 5413, 5445, 5477, 5509, 5541, 5573, 5605,
    5637, 5669, 5701, 5733, 5765, 5797, 5830, 5862,
    5894, 5926, 5958, 5990, 6022, 6054, 6086, 6118,
    6150, 6182, 6214, 6246, 6278, 6310, 6342, 6374,
    6406, 6438, 6470, 6502, 6534, 6566, 6598, 6630,
    6662, 6694, 6726, 6758, 6790, 6822, 6854, 6887,
    6919, 6951, 6983, 7015, 7047, 7079, 7111, 7143,
    7175, 7207, 7239, 7271, 7303, 7335, 7367, 7399,
    7431, 7463, 7495, 7527, 7559, 7591, 7623, 7655,
    7687, 7719, 7751, 7783, 7815, 7847, 7879, 7911,
    7944, 7976, 8008, 8040, 8072, 8104, 8136, 8168,
    8200, 8232, 8264, 8296, 8328, 8360, 8392, 8424,
    8456, 8488, 8520, 8552, 8584, 8616, 8648, 8680,
    8712, 8744, 8776, 8808, 8840, 8872, 8904, 8936,
    8968, 9001, 9033, 9065, 9097, 9129, 9161, 9193,
    9225, 9257, 9289, 9321, 9353, 9385, 9417, 9449,
    9481, 9513, 9545, 9577, 9609, 9641, 9673, 9705,
    9737, 9769, 9801, 9833, 9865, 9897, 9929, 9961,
    9993, 10025, 10058, 10090, 10122, 10154, 10186, 10218,
    10250, 10282, 10314, 10346, 10378, 10410, 10442, 10474,
    10506, 10538, 10570, 10602, 10634, 10666, 10698, 10730,
    10762, 10794, 10826, 10858, 10890, 10922, 10954, 10986,
    11018, 11050, 11082, 11115, 11147, 11179, 11211, 11243,
    11275, 11307, 11339, 11371, 11403, 11435, 11467, 11499,
    11531, 11563, 11595, 11627, 11659, 11691, 11723, 11755,
    11787, 11819, 11851, 11883, 11915, 11947, 11979, 12011,
    12043, 12075, 12107, 12139, 12172, 12204, 12236, 12268,
    12300, 12332, 12364, 12396, 12428, 12460, 12492, 12524,
    12556, 12588, 12620, 12652, 12684, 12716, 12748, 12780,
    12812, 12844, 12876, 12908, 12940, 12972, 13004, 13036,
    13068, 13100, 13132, 13164, 13196, 13229, 13261, 13293,
    13325, 13357, 13389, 13421, 13453, 13485, 13517, 13549,
    13581, 13613, 13645, 13677, 13709, 13741, 13773, 13805,
    13837, 13869, 13901, 13933, 13965, 13997, 14029, 14061,
    14093, 14125, 14157, 14189, 14221, 14253, 14286, 14318,
    14350, 14382, 14414, 14446, 14478, 14510, 14542, 14574,
    14606, 14638, 14670, 14702, 14734, 14766, 14798, 14830,
    14862, 14894, 14926, 14958, 14990, 15022, 15054, 15086,
    15118, 15150, 15182, 15214, 15246, 15278, 15310, 15343,
    15375, 15407, 15439, 15471, 15503, 15535, 15567, 15599,
    15631, 15663, 15695, 15727, 15759, 15791, 15823, 15855,
    15887, 15919, 15951, 15983, 16015, 16047, 16079, 16111,
    16143, 16175, 16207, 16239, 16271, 16303, 16335, 16367,
    16400, 16432, 16464, 16496, 16528, 16560, 16592, 16624,
    16656, 16688, 16720, 16752, 16784, 16816, 16848, 16880,
    16912, 16944, 16976, 17008, 17040, 17072, 17104, 17136,
    17168, 17200, 17232, 17264, 17296, 17328, 17360, 17392,
    17424, 17457, 17489, 17521, 17553, 17585, 17617, 17649,
    17681, 17713, 17745, 17777, 17809, 17841, 17873, 17905,
    17937, 17969, 18001, 18033, 18065, 18097, 18129, 18161,
    18193, 18225, 18257, 18289, 18321, 18353, 18385, 18417,
    18449, 18481, 18514, 18546, 18578, 18610, 18642, 18674,
    18706, 18738, 18770, 18802, 18834, 18866, 18898, 18930,
    18962, 18994, 19026, 19058, 19090, 19122, 19154, 19186,
    19218, 19250, 19282, 19314, 19346, 19378, 19410, 19442,
    19474, 19506, 19538, 19571, 19603, 19635, 19667, 19699,
    19731, 19763, 19795, 19827, 19859, 19891, 19923, 19955,
    19987, 20019, 20051, 20083, 20115, 20147, 20179, 20211,
    20243, 20275, 20307, 20339, 20371, 20403, 20435, 20467,
    20499, 20531, 20563, 20595, 20628, 20660, 20692, 20724,
    20756, 20788, 20820, 20852, 20884, 20916, 20948, 20980,
    21012, 21044, 21076, 21108, 21140, 21172, 21204, 21236,
    21268, 21300, 21332, 21364, 21396, 21428, 21460, 21492,
    21524, 21556, 21588, 21620, 21652, 21685, 21717, 21749,
    21781, 21813, 21845, 21877, 21909, 21941, 21973, 22005,
    22037, 22069, 22101, 22133, 22165, 22197, 22229, 22261,
    22293, 22325, 22357, 22389, 22421, 22453, 22485, 22517,
    22549, 22581, 22613, 22645, 22677, 22709, 22742, 22774,
    22806, 22838, 22870, 22902, 22934, 22966, 22998, 23030,
    23062, 23094, 23126, 23158, 23190, 23222, 23254, 23286,
    23318, 23350, 23382, 23414, 23446, 23478, 23510, 23542,
    23574, 23606, 23638, 23670, 23702, 23734, 23766, 23799,
    23831, 23863, 23895, 23927, 23959, 23991, 24023, 24055,
    24087, 24119, 24151, 24183, 24215, 24247, 24279, 24311,
    24343, 24375, 24407, 24439, 24471, 24503, 24535, 24567,
    24599, 24631, 24663, 24695, 24727, 24759, 24791, 24823,
    24856, 24888, 24920, 24952, 24984, 25016, 25048, 25080,
    25112, 25144, 25176, 25208, 25240, 25272, 25304, 25336,
    25368, 25400, 25432, 25464, 25496, 25528, 25560, 25592,
    25624, 25656, 25688, 25720, 25752, 25784, 25816, 25848,
    25880, 25913, 25945, 25977, 26009, 26041, 26073, 26105,
    26137, 26169, 26201, 26233, 26265, 26297, 26329, 26361,
    26393, 26425, 26457, 26489, 26521, 26553, 26585, 26617,
    26649, 26681, 26713, 26745, 26777, 26809, 26841, 26873,
    26905, 26937, 26970, 27002, 27034, 27066, 27098, 27130,
    27162, 27194, 27226, 27258, 27290, 27322, 27354, 27386,
    27418, 27450, 27482, 27514, 27546, 27578, 27610, 27642,
    27674, 27706, 27738, 27770, 27802, 27834, 27866, 27898,
    27930, 27962, 27994, 28027, 28059, 28091, 28123, 28155,
    28187, 28219, 28251, 28283, 28315, 28347, 28379, 28411,
    28443, 28475, 28507, 28539, 28571, 28603, 28635, 28667,
    28699, 28731, 28763, 28795, 28827, 28859, 28891, 28923,
    28955, 28987, 29019, 29051, 29084, 29116, 29148, 29180,
    29212, 29244, 29276, 29308, 29340, 29372, 29404, 29436,
    29468, 29500, 29532, 29564, 29596, 29628, 29660, 29692,
    29724, 29756, 29788, 29820, 29852, 29884, 29916, 29948,
    29980, 30012, 30044, 30076, 30108, 30141, 30173, 30205,
    30237, 30269, 30301, 30333, 30365, 30397, 30429, 30461,
    30493, 30525, 30557, 30589, 30621, 30653, 30685, 30717,
    30749, 30781, 30813, 30845, 30877, 30909, 30941, 30973,
    31005, 31037, 31069, 31101, 31133, 31165, 31198, 31230,
    31262, 31294, 31326, 31358, 31390, 31422, 31454, 31486,
    31518, 31550, 31582, 31614, 31646, 31678, 31710, 31742,
    31774, 31806, 31838, 31870, 31902, 31934, 31966, 31998,
    32030, 32062, 32094, 32126, 32158, 32190, 32222, 32255,
    32287, 32319, 32351, 32383, 32415, 32447, 32479, 32511,
    32543, 32575, 32607, 32639, 32671, 32703, 32735, 32767
};

static uint16_t* revgamma_array = (uint16_t*) srgb_revgamma_array;

static inline void kd3_revgamma_transform(kd3_color* x) {
    x->a[0] = revgamma_array[x->a[0] >> 5];
    x->a[1] = revgamma_array[x->a[1] >> 5];
    x->a[2] = revgamma_array[x->a[2] >> 5];
}

void kd3_set_gamma(int type, double gamma) {
#if HAVE_POW
    static int cur_type = KD3_GAMMA_SRGB;
    static double cur_gamma = 2.2;
    int i;
    if (type == cur_type && (type != KD3_GAMMA_NUMERIC || gamma == cur_gamma))
        return;
    if (type == KD3_GAMMA_SRGB) {
        if (gamma_array != srgb_gamma_array) {
            Gif_DeleteArray(gamma_array);
            Gif_DeleteArray(revgamma_array);
        }
        gamma_array = (uint16_t*) srgb_gamma_array;
        revgamma_array = (uint16_t*) srgb_revgamma_array;
    } else {
        if (gamma_array == srgb_gamma_array) {
            gamma_array = Gif_NewArray(uint16_t, 1024);
            revgamma_array = Gif_NewArray(uint16_t, 1024);
        }
        for (i = 0; i < 1024; ++i) {
            double x = (double) i / 1023;
            gamma_array[i] = (int) (pow(x, gamma) * 32767);
            while (i && gamma_array[i] < gamma_array[i-1]
                   && gamma_array[i] < 32767)
                ++i;
            revgamma_array[i] = (int) (pow(x, 1/gamma) * 32767);
            while (i && revgamma_array[i] < revgamma_array[i-1]
                   && revgamma_array[i] < 32767)
                ++i;
        }
    }
    cur_type = type;
    cur_gamma = gamma;
#else
    (void) type, (void) gamma;
#endif
}


static inline int kd3_luminance(const kd3_color* x) {
    return (306 * x->a[0] + 601 * x->a[1] + 117 * x->a[2]) >> 10;
}

static inline void kd3_luminance_gamma_transform(kd3_color* x) {
    /* For grayscale colormaps, use distance in luminance space instead of
       distance in RGB space. The weights for the R,G,B components in
       luminance space are 0.299,0.587,0.114. Using the proportional factors
       306, 601, and 117 we get a scaled gray value between 0 and 255 *
       1024. Thanks to Christian Kumpf, <kumpf@igd.fhg.de>, for providing a
       patch. */
    x->a[0] = x->a[1] = x->a[2] = gamma_array[kd3_luminance(x) >> 7];
}

typedef struct Gif_Histogram {
  Gif_Color *c;
  int n;
  int cap;
} Gif_Histogram;

static void add_histogram_color(Gif_Color *, Gif_Histogram *, unsigned long);

static void
init_histogram(Gif_Histogram *new_hist, Gif_Histogram *old_hist)
{
  int new_cap = (old_hist ? old_hist->cap * 2 : 1024);
  Gif_Color *nc = Gif_NewArray(Gif_Color, new_cap);
  int i;
  new_hist->c = nc;
  new_hist->n = 0;
  new_hist->cap = new_cap;
  for (i = 0; i < new_cap; i++)
    new_hist->c[i].haspixel = 0;
  if (old_hist)
    for (i = 0; i < old_hist->cap; i++)
      if (old_hist->c[i].haspixel)
	add_histogram_color(&old_hist->c[i], new_hist, old_hist->c[i].pixel);
}

static void
delete_histogram(Gif_Histogram *hist)
{
  Gif_DeleteArray(hist->c);
}

static void
add_histogram_color(Gif_Color *color, Gif_Histogram *hist, unsigned long count)
{
  Gif_Color *hc = hist->c;
  int hcap = hist->cap - 1;
  int i = (((color->gfc_red & 0xF0) << 4) | (color->gfc_green & 0xF0)
	   | (color->gfc_blue >> 4)) & hcap;
  int hash2 = ((((color->gfc_red & 0x0F) << 8) | ((color->gfc_green & 0x0F) << 4)
		| (color->gfc_blue & 0x0F)) & hcap) | 1;

  for (; hc[i].haspixel; i = (i + hash2) & hcap)
    if (GIF_COLOREQ(&hc[i], color)) {
      hc[i].pixel += count;
      color->haspixel = 1;
      color->pixel = i;
      return;
    }

  if (hist->n > ((hist->cap * 7) >> 3)) {
    Gif_Histogram new_hist;
    init_histogram(&new_hist, hist);
    delete_histogram(hist);
    *hist = new_hist;
    hc = hist->c;		/* 31.Aug.1999 - bug fix from Steven Marthouse
				   <comments@vrml3d.com> */
  }

  hist->n++;
  hc[i] = *color;
  hc[i].haspixel = 1;
  hc[i].pixel = count;
  color->haspixel = 1;
  color->pixel = i;
}

static int
popularity_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return b->pixel - a->pixel;
}

static int
pixel_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->pixel - b->pixel;
}


Gif_Color *
histogram(Gif_Stream *gfs, int *nhist_store)
{
  Gif_Histogram hist;
  Gif_Color *linear;
  Gif_Color transparent_color;
  unsigned long ntransparent = 0;
  unsigned long nbackground = 0;
  int x, y, i;

  unmark_colors(gfs->global);
  for (i = 0; i < gfs->nimages; i++)
    unmark_colors(gfs->images[i]->local);

  init_histogram(&hist, 0);

  /* Count pixels. Be careful about values which are outside the range of the
     colormap. */
  for (i = 0; i < gfs->nimages; i++) {
    Gif_Image *gfi = gfs->images[i];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    uint32_t count[256];
    Gif_Color *col;
    int ncol;
    int transparent = gfi->transparent;
    int only_compressed = (gfi->img == 0);
    if (!gfcm) continue;

    /* unoptimize the image if necessary */
    if (only_compressed)
      Gif_UncompressImage(gfi);

    /* sweep over the image data, counting pixels */
    for (x = 0; x < 256; x++)
      count[x] = 0;
    for (y = 0; y < gfi->height; y++) {
      uint8_t *data = gfi->img[y];
      for (x = 0; x < gfi->width; x++, data++)
	count[*data]++;
    }

    /* add counted colors to global histogram */
    col = gfcm->col;
    ncol = gfcm->ncol;
    for (x = 0; x < ncol; x++)
      if (count[x] && x != transparent) {
	if (col[x].haspixel)
	  hist.c[ col[x].pixel ].pixel += count[x];
	else
	  add_histogram_color(&col[x], &hist, count[x]);
      }
    if (transparent >= 0) {
      if (ntransparent == 0)
	  transparent_color = col[transparent];
      ntransparent += count[transparent];
    }

    /* if this image has background disposal, count its size towards the
       background's pixel count */
    if (gfi->disposal == GIF_DISPOSAL_BACKGROUND)
      nbackground += gfi->width * gfi->height;

    /* unoptimize the image if necessary */
    if (only_compressed)
      Gif_ReleaseUncompressedImage(gfi);
  }

  /* account for background by adding it to 'ntransparent' or the histogram */
  if (gfs->images[0]->transparent < 0 && gfs->global
      && gfs->background < gfs->global->ncol)
    add_histogram_color(&gfs->global->col[gfs->background], &hist, nbackground);
  else
    ntransparent += nbackground;

  /* now, make the linear histogram from the hashed histogram */
  linear = Gif_NewArray(Gif_Color, hist.n + 1);
  i = 0;

  /* Put all transparent pixels in histogram slot 0. Transparent pixels are
     marked by haspixel == 255. */
  if (ntransparent) {
    linear[0] = transparent_color;
    linear[0].haspixel = 255;
    linear[0].pixel = ntransparent;
    i++;
  }

  /* put hash histogram colors into linear histogram */
  for (x = 0; x < hist.cap; x++)
    if (hist.c[x].haspixel)
      linear[i++] = hist.c[x];

  delete_histogram(&hist);
  *nhist_store = i;
  return linear;
}


#undef min
#undef max
#define min(a, b)	((a) < (b) ? (a) : (b))
#define max(a, b)	((a) > (b) ? (a) : (b))

static int
red_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->gfc_red - b->gfc_red;
}

static int
green_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->gfc_green - b->gfc_green;
}

static int
blue_sort_compare(const void *va, const void *vb)
{
  const Gif_Color *a = (const Gif_Color *)va;
  const Gif_Color *b = (const Gif_Color *)vb;
  return a->gfc_blue - b->gfc_blue;
}


static void
assert_hist_transparency(Gif_Color *hist, int nhist)
{
  int i;
  for (i = 1; i < nhist; i++)
    assert(hist[i].haspixel != 255);
}


/* COLORMAP FUNCTIONS return a palette (a vector of Gif_Colors). The
   pixel fields are undefined; the haspixel fields are all 0. */

typedef struct {
  int first;
  int count;
  uint32_t pixel;
} adaptive_slot;

Gif_Colormap *
colormap_median_cut(Gif_Color* hist, int nhist, Gt_OutputData* od)
{
  int adapt_size = od->colormap_size;
  adaptive_slot *slots = Gif_NewArray(adaptive_slot, adapt_size);
  Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size, 256);
  Gif_Color *adapt = gfcm->col;
  int nadapt;
  int i, j;

  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */

  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size >= nhist && !od->colormap_fixed)
    warning(1, "trivial adaptive palette (only %d colors in source)", nhist);
  if (adapt_size >= nhist)
    adapt_size = nhist;

  /* 0. remove any transparent color from consideration; reduce adaptive
     palette size to accommodate transparency if it looks like that'll be
     necessary */
  assert_hist_transparency(hist, nhist);
  if (adapt_size > 2 && adapt_size < nhist && hist[0].haspixel == 255
      && nhist <= 265)
    adapt_size--;
  if (hist[0].haspixel == 255) {
    hist[0] = hist[nhist - 1];
    nhist--;
  }

  /* 1. set up the first slot, containing all pixels. */
  {
    uint32_t total = 0;
    for (i = 0; i < nhist; i++)
      total += hist[i].pixel;
    slots[0].first = 0;
    slots[0].count = nhist;
    slots[0].pixel = total;
    qsort(hist, nhist, sizeof(Gif_Color), pixel_sort_compare);
  }

  /* 2. split slots until we have enough. */
  for (nadapt = 1; nadapt < adapt_size; nadapt++) {
    adaptive_slot *split = 0;
    Gif_Color minc, maxc, *slice;

    /* 2.1. pick the slot to split. */
    {
      uint32_t split_pixel = 0;
      for (i = 0; i < nadapt; i++)
	if (slots[i].count >= 2 && slots[i].pixel > split_pixel) {
	  split = &slots[i];
	  split_pixel = slots[i].pixel;
	}
      if (!split)
	break;
    }
    slice = &hist[split->first];

    /* 2.2. find its extent. */
    {
      Gif_Color *trav = slice;
      minc = maxc = *trav;
      for (i = 1, trav++; i < split->count; i++, trav++) {
	minc.gfc_red = min(minc.gfc_red, trav->gfc_red);
	maxc.gfc_red = max(maxc.gfc_red, trav->gfc_red);
	minc.gfc_green = min(minc.gfc_green, trav->gfc_green);
	maxc.gfc_green = max(maxc.gfc_green, trav->gfc_green);
	minc.gfc_blue = min(minc.gfc_blue, trav->gfc_blue);
	maxc.gfc_blue = max(maxc.gfc_blue, trav->gfc_blue);
      }
    }

    /* 2.3. decide how to split it. use the luminance method. also sort the
       colors. */
    {
      double red_diff = 0.299 * (maxc.gfc_red - minc.gfc_red);
      double green_diff = 0.587 * (maxc.gfc_green - minc.gfc_green);
      double blue_diff = 0.114 * (maxc.gfc_blue - minc.gfc_blue);
      if (red_diff >= green_diff && red_diff >= blue_diff)
	qsort(slice, split->count, sizeof(Gif_Color), red_sort_compare);
      else if (green_diff >= blue_diff)
	qsort(slice, split->count, sizeof(Gif_Color), green_sort_compare);
      else
	qsort(slice, split->count, sizeof(Gif_Color), blue_sort_compare);
    }

    /* 2.4. decide where to split the slot and split it there. */
    {
      uint32_t half_pixels = split->pixel / 2;
      uint32_t pixel_accum = slice[0].pixel;
      uint32_t diff1, diff2;
      for (i = 1; i < split->count - 1 && pixel_accum < half_pixels; i++)
	pixel_accum += slice[i].pixel;

      /* We know the area before the split has more pixels than the area
         after, possibly by a large margin (bad news). If it would shrink the
         margin, change the split. */
      diff1 = 2*pixel_accum - split->pixel;
      diff2 = split->pixel - 2*(pixel_accum - slice[i-1].pixel);
      if (diff2 < diff1 && i > 1) {
	i--;
	pixel_accum -= slice[i].pixel;
      }

      slots[nadapt].first = split->first + i;
      slots[nadapt].count = split->count - i;
      slots[nadapt].pixel = split->pixel - pixel_accum;
      split->count = i;
      split->pixel = pixel_accum;
    }
  }

  /* 3. make the new palette by choosing one color from each slot. */
  for (i = 0; i < nadapt; i++) {
    double red_total = 0, green_total = 0, blue_total = 0;
    Gif_Color *slice = &hist[ slots[i].first ];
    kd3_color k;
    for (j = 0; j < slots[i].count; j++) {
        kd3_set8(&k, slice[j].gfc_red, slice[j].gfc_green, slice[j].gfc_blue);
        kd3_gamma_transform(&k);
        red_total += k.a[0] * (double) slice[j].pixel;
        green_total += k.a[1] * (double) slice[j].pixel;
        blue_total += k.a[2] * (double) slice[j].pixel;
    }
    k.a[0] = (int) (red_total / slots[i].pixel);
    k.a[1] = (int) (green_total / slots[i].pixel);
    k.a[2] = (int) (blue_total / slots[i].pixel);
    kd3_revgamma_transform(&k);
    adapt[i].gfc_red = (uint8_t) (k.a[0] >> 7);
    adapt[i].gfc_green = (uint8_t) (k.a[1] >> 7);
    adapt[i].gfc_blue = (uint8_t) (k.a[2] >> 7);
    adapt[i].haspixel = 0;
  }

  Gif_DeleteArray(slots);
  gfcm->ncol = nadapt;
  return gfcm;
}



static Gif_Colormap *
colormap_diversity(Gif_Color *hist, int nhist, Gt_OutputData* od,
                   int blend)
{
  int adapt_size = od->colormap_size;
  uint32_t* min_dist = Gif_NewArray(uint32_t, nhist);
  uint32_t* min_dither_dist = Gif_NewArray(uint32_t, nhist);
  int *closest = Gif_NewArray(int, nhist);
  kd3_color* gchist = Gif_NewArray(kd3_color, nhist); /* gamma-corrected */
  Gif_Colormap *gfcm = Gif_NewFullColormap(adapt_size, 256);
  Gif_Color *adapt = gfcm->col;
  int nadapt = 0;
  int i, j, match = 0;

  /* This code was uses XV's modified diversity algorithm, and was written
     with reference to XV's implementation of that algorithm by John Bradley
     <bradley@cis.upenn.edu> and Tom Lane <Tom.Lane@g.gp.cs.cmu.edu>. */

  if (adapt_size < 2 || adapt_size > 256)
    fatal_error("adaptive palette size must be between 2 and 256");
  if (adapt_size > nhist && !od->colormap_fixed)
    warning(1, "trivial adaptive palette (only %d colors in source)", nhist);
  if (adapt_size > nhist)
    adapt_size = nhist;

  /* 0. remove any transparent color from consideration; reduce adaptive
     palette size to accommodate transparency if it looks like that'll be
     necessary */
  assert_hist_transparency(hist, nhist);
  /* It will be necessary to accommodate transparency if (1) there is
     transparency in the image; (2) the adaptive palette isn't trivial; and
     (3) there are a small number of colors in the image (arbitrary constant:
     <= 265), so it's likely that most images will use most of the slots, so
     it's likely there won't be unused slots. */
  if (adapt_size > 2 && adapt_size < nhist && hist[0].haspixel == 255
      && nhist <= 265)
    adapt_size--;
  if (hist[0].haspixel == 255) {
    hist[0] = hist[nhist - 1];
    nhist--;
  }

  /* blending has bad effects when there are very few colors */
  if (adapt_size < 4)
    blend = 0;

  /* 1. initialize min_dist and sort the colors in order of popularity. */
  for (i = 0; i < nhist; i++)
      min_dist[i] = min_dither_dist[i] = 0xFFFFFFFF;

  qsort(hist, nhist, sizeof(Gif_Color), popularity_sort_compare);

  /* 1.5. gamma-correct hist colors */
  for (i = 0; i < nhist; ++i) {
      kd3_set8(&gchist[i], hist[i].gfc_red, hist[i].gfc_green,
               hist[i].gfc_blue);
      kd3_gamma_transform(&gchist[i]);
  }

  /* 2. choose colors one at a time */
  for (nadapt = 0; nadapt < adapt_size; nadapt++) {
    int chosen;

    /* 2.0. find the first non-chosen color */
    for (chosen = 0; min_dist[chosen] == 0; ++chosen)
        /* do nothing */;

    /* 2.1. choose the color to be added */
    if (nadapt == 0 || (nadapt >= 10 && nadapt % 2 == 0)) {
        /* 2.1a. want most popular unchosen color; we've sorted them on
           popularity, so we're done! */

    } else if (!od->colormap_dither) {
        /* 2.1b. choose based on diversity from unchosen colors */
        for (i = chosen + 1; i != nhist; ++i)
            if (min_dist[i] > min_dist[chosen])
                chosen = i;

    } else {
        /* 2.1c. choose based on diversity from unchosen colors, but allow
           dithered combinations to stand in for colors, particularly early
           on in the color finding process */
#if HAVE_POW
        /* Weight assigned to dithered combinations drops as we proceed. */
        double dweight = 0.05 + pow(0.25, 1 + (nadapt - 1) / 3.);
#else
        double dweight = nadapt < 4 ? 0.25 : 0.125;
#endif
        double max_dist = min_dist[chosen] + min_dither_dist[chosen] * dweight;
        for (i = chosen + 1; i != nhist; ++i)
            if (min_dist[i] != 0) {
                double dist = min_dist[i] + min_dither_dist[i] * dweight;
                if (dist > max_dist) {
                    chosen = i;
                    max_dist = dist;
                }
            }
    }

    /* 2.2. add the color */
    min_dist[chosen] = min_dither_dist[chosen] = 0;
    closest[chosen] = nadapt;
    adapt[nadapt] = hist[chosen];
    adapt[nadapt].pixel = chosen;
    adapt[nadapt].haspixel = 0;

    /* 2.3. adjust the min_dist array */
    for (i = 0; i < nhist; ++i)
        if (min_dist[i]) {
            uint32_t dist = kd3_distance(&gchist[i], &gchist[chosen]);
            if (dist < min_dist[i]) {
                min_dist[i] = dist;
                closest[i] = nadapt;
            }
        }

    /* 2.4. also account for dither distances */
    if (od->colormap_dither && nadapt > 0 && nadapt < 64)
        for (j = 0; j < nadapt; ++j) {
            kd3_color x = gchist[chosen], *y = &gchist[adapt[j].pixel];
            /* penalize combinations with large luminance difference */
            double dL = fabs(kd3_luminance(&x) - kd3_luminance(y));
            dL = (dL > 8192 ? dL * 4 / 32767. : 1);
            /* create combination */
            for (i = 0; i < 3; ++i)
                x.a[i] = (x.a[i] + y->a[i]) >> 1;
            /* track closeness of combination to other colors */
            for (i = 0; i < nhist; ++i)
                if (min_dist[i]) {
                    double dist = kd3_distance(&gchist[i], &x) * dL;
                    if (dist < min_dither_dist[i])
                        min_dither_dist[i] = (uint32_t) dist;
                }
        }
  }

  /* 3. make the new palette by choosing one color from each slot. */
  if (blend) {
    for (i = 0; i < nadapt; i++) {
      double red_total = 0, green_total = 0, blue_total = 0;
      uint32_t pixel_total = 0, mismatch_pixel_total = 0;
      for (j = 0; j < nhist; j++)
	if (closest[j] == i) {
	  double pixel = hist[j].pixel;
	  red_total += gchist[j].a[0] * pixel;
	  green_total += gchist[j].a[1] * pixel;
	  blue_total += gchist[j].a[2] * pixel;
	  pixel_total += pixel;
	  if (min_dist[j])
	    mismatch_pixel_total += pixel;
	  else
	    match = j;
	}
      /* Only blend if total number of mismatched pixels exceeds total number
	 of matched pixels by a large margin. */
      if (3 * mismatch_pixel_total <= 2 * pixel_total)
	adapt[i] = hist[match];
      else {
	/* Favor, by a smallish amount, the color the plain diversity
           algorithm would pick. */
	double pixel = hist[match].pixel * 2;
        kd3_color k;
	red_total += gchist[match].a[0] * pixel;
	green_total += gchist[match].a[1] * pixel;
	blue_total += gchist[match].a[2] * pixel;
	pixel_total += pixel;
        k.a[0] = (int) (red_total / pixel_total);
        k.a[1] = (int) (green_total / pixel_total);
        k.a[2] = (int) (blue_total / pixel_total);
        kd3_revgamma_transform(&k);
	adapt[i].gfc_red = (uint8_t) (k.a[0] >> 7);
	adapt[i].gfc_green = (uint8_t) (k.a[1] >> 7);
	adapt[i].gfc_blue = (uint8_t) (k.a[2] >> 7);
      }
      adapt[i].haspixel = 0;
    }
  }

  Gif_DeleteArray(min_dist);
  Gif_DeleteArray(min_dither_dist);
  Gif_DeleteArray(closest);
  Gif_DeleteArray(gchist);
  gfcm->ncol = nadapt;
  return gfcm;
}


Gif_Colormap* colormap_blend_diversity(Gif_Color* hist, int nhist,
                                       Gt_OutputData* od) {
    return colormap_diversity(hist, nhist, od, 1);
}

Gif_Colormap* colormap_flat_diversity(Gif_Color* hist, int nhist,
                                      Gt_OutputData* od) {
    return colormap_diversity(hist, nhist, od, 0);
}


/*****
 * kd_tree allocation and deallocation
 **/

typedef struct kd3_item {
    kd3_color k;
    int index;
} kd3_item;

typedef struct kd3_treepos {
    int pivot;
    int offset;
} kd3_treepos;

struct kd3_tree {
    kd3_treepos* tree;
    int ntree;
    int disabled;
    kd3_item* items;
    int nitems;
    int items_cap;
    int maxdepth;
    unsigned (*distance)(const kd3_color*, const kd3_color*);
    void (*transform)(kd3_color*);
    unsigned* xradius;
};

void kd3_init(kd3_tree* kd3,
              unsigned (*distance)(const kd3_color*, const kd3_color*),
              void (*transform)(kd3_color*)) {
    kd3->tree = NULL;
    kd3->items = Gif_NewArray(kd3_item, 256);
    kd3->nitems = 0;
    kd3->items_cap = 256;
    kd3->distance = distance;
    kd3->transform = transform;
    kd3->xradius = NULL;
    kd3->disabled = -1;
}

void kd3_cleanup(kd3_tree* kd3) {
    Gif_DeleteArray(kd3->tree);
    Gif_DeleteArray(kd3->items);
    Gif_DeleteArray(kd3->xradius);
}

void kd3_add(kd3_tree* kd3, kd3_color k) {
    assert(!kd3->tree);
    if (kd3->nitems == kd3->items_cap) {
        kd3->items_cap *= 2;
        Gif_ReArray(kd3->items, kd3_item, kd3->items_cap);
    }
    kd3->items[kd3->nitems].k = k;
    if (kd3->transform)
        kd3->transform(&kd3->items[kd3->nitems].k);
    kd3->items[kd3->nitems].index = kd3->nitems;
    ++kd3->nitems;
}

void kd3_add8(kd3_tree* kd3, int a0, int a1, int a2) {
    kd3_color k;
    kd3_set8(&k, a0, a1, a2);
    kd3_add(kd3, k);
}

static int kd3_item_compar_0(const void* a, const void* b) {
    const kd3_item* aa = (const kd3_item*) a, *bb = (const kd3_item*) b;
    return aa->k.a[0] - bb->k.a[0];
}

static int kd3_item_compar_1(const void* a, const void* b) {
    const kd3_item* aa = (const kd3_item*) a, *bb = (const kd3_item*) b;
    return aa->k.a[1] - bb->k.a[1];
}

static int kd3_item_compar_2(const void* a, const void* b) {
    const kd3_item* aa = (const kd3_item*) a, *bb = (const kd3_item*) b;
    return aa->k.a[2] - bb->k.a[2];
}

static int (*kd3_item_compars[])(const void*, const void*) = {
    &kd3_item_compar_0, &kd3_item_compar_1, &kd3_item_compar_2
};

static int kd3_build_range(kd3_tree* kd3, kd3_item* items,
                           int l, int r, int n, int depth) {
    int m, aindex = depth % 3;
    if (depth > kd3->maxdepth)
        kd3->maxdepth = depth;
    while (n >= kd3->ntree) {
        kd3->ntree *= 2;
        Gif_ReArray(kd3->tree, kd3_treepos, kd3->ntree);
    }
    if (l + 1 >= r) {
        kd3->tree[n].pivot = (l == r ? -1 : items[l].index);
        kd3->tree[n].offset = -1;
        return 2;
    }

    qsort(&items[l], r - l, sizeof(kd3_item), kd3_item_compars[aindex]);
    m = l + ((r - l) >> 1);
    while (m > l && items[m].k.a[aindex] == items[m-1].k.a[aindex])
        --m;
    if (m == l)
        kd3->tree[n].pivot = items[m].k.a[aindex];
    else
        kd3->tree[n].pivot = items[m-1].k.a[aindex]
            + ((items[m].k.a[aindex] - items[m-1].k.a[aindex]) >> 1);
    int nl = kd3_build_range(kd3, items, l, m, n+1, depth+1);
    kd3->tree[n].offset = 1+nl;
    int nr = kd3_build_range(kd3, items, m, r, n+1+nl, depth+1);
    return 1+nl+nr;
}

static int kd3_item_all_compar(const void* a, const void* b) {
    const kd3_item* aa = (const kd3_item*) a, *bb = (const kd3_item*) b;
    if (aa->k.a[0] - bb->k.a[0])
        return aa->k.a[0] - bb->k.a[0];
    else if (aa->k.a[1] - bb->k.a[1])
        return aa->k.a[1] - bb->k.a[1];
    else
        return aa->k.a[2] - bb->k.a[2];
}

void kd3_print(kd3_tree* kd3, int depth, kd3_treepos* p, int* a, int* b) {
    int i;
    char x[6][10];
    for (i = 0; i != 3; ++i) {
        if (a[i] == 0x80000000)
            sprintf(x[2*i], "*");
        else
            sprintf(x[2*i], "%d", a[i]);
        if (b[i] == 0x7FFFFFFF)
            sprintf(x[2*i+1], "*");
        else
            sprintf(x[2*i+1], "%d", b[i]);
    }
    printf("%*s<%s:%s,%s:%s,%s:%s>", depth*3, "",
           x[0], x[1], x[2], x[3], x[4], x[5]);
    if (p->offset < 0) {
        if (p->pivot >= 0) {
            assert(kd3->items[p->pivot].k.a[0] >= a[0]);
            assert(kd3->items[p->pivot].k.a[1] >= a[1]);
            assert(kd3->items[p->pivot].k.a[2] >= a[2]);
            assert(kd3->items[p->pivot].k.a[0] < b[0]);
            assert(kd3->items[p->pivot].k.a[1] < b[1]);
            assert(kd3->items[p->pivot].k.a[2] < b[2]);
            printf(" ** @%d: <%d,%d,%d>\n", p->pivot, kd3->items[p->pivot].k.a[0], kd3->items[p->pivot].k.a[1], kd3->items[p->pivot].k.a[2]);
        }
    } else {
        int aindex = depth % 3;
        assert(p->pivot >= a[aindex]);
        assert(p->pivot < b[aindex]);
        printf((aindex == 0 ? " | <%d,_,_>\n" :
                aindex == 1 ? " | <_,%d,_>\n" : " | <_,_,%d>\n"), p->pivot);
        int x[3];
        memcpy(x, b, sizeof(int) * 3);
        x[aindex] = p->pivot;
        kd3_print(kd3, depth + 1, p + 1, a, x);
        memcpy(x, a, sizeof(int) * 3);
        x[aindex] = p->pivot;
        kd3_print(kd3, depth + 1, p + p->offset, x, b);
    }
}

void kd3_build(kd3_tree* kd3) {
    kd3_item* items;
    int i, j, delta;
    assert(!kd3->tree);

    /* create xradius */
    kd3->xradius = Gif_NewArray(unsigned, kd3->nitems);
    for (i = 0; i != kd3->nitems; ++i)
        kd3->xradius[i] = (unsigned) -1;
    for (i = 0; i != kd3->nitems; ++i)
        for (j = i + 1; j != kd3->nitems; ++j) {
            unsigned dist = kd3->distance(&kd3->items[i].k, &kd3->items[j].k);
            unsigned radius = dist / 4;
            if (radius < kd3->xradius[i])
                kd3->xradius[i] = radius;
            if (radius < kd3->xradius[j])
                kd3->xradius[j] = radius;
        }

    /* create tree */
    kd3->tree = Gif_NewArray(kd3_treepos, 256);
    kd3->ntree = 256;
    kd3->maxdepth = 0;

    /* create copy of items; remove duplicates */
    items = Gif_NewArray(kd3_item, kd3->nitems);
    memcpy(items, kd3->items, sizeof(kd3_item) * kd3->nitems);
    qsort(items, kd3->nitems, sizeof(kd3_item), kd3_item_all_compar);
    for (i = 0, delta = 1; i != kd3->nitems - delta; ++i)
        if (items[i].k.a[0] == items[i+delta].k.a[0]
            && items[i].k.a[1] == items[i+delta].k.a[1]
            && items[i].k.a[2] == items[i+delta].k.a[2])
            ++delta, --i;
        else if (delta > 1)
            items[i+1] = items[i+delta];

    kd3_build_range(kd3, items, 0, kd3->nitems - (delta - 1), 0, 0);
    assert(kd3->maxdepth < 32);

    Gif_DeleteArray(items);
}

void kd3_disable(kd3_tree* kd3, int i) {
    assert(kd3->disabled < 0);
    kd3->disabled = i;
}

void kd3_enable_all(kd3_tree* kd3) {
    kd3->disabled = -1;
}

int kd3_closest_transformed(const kd3_tree* kd3, const kd3_color* k) {
    assert(kd3->tree);
    const kd3_treepos* stack[32];
    uint8_t state[32];
    int stackpos = 0;
    int result = -1;
    unsigned mindist = (unsigned) -1;
    stack[0] = kd3->tree;
    state[0] = 0;

    while (stackpos >= 0) {
        assert(stackpos < 32);
        const kd3_treepos* p = stack[stackpos];

        if (p->offset < 0) {
            if (p->pivot >= 0 && kd3->disabled != p->pivot) {
                unsigned dist = kd3->distance(&kd3->items[p->pivot].k, k);
                if (dist < mindist) {
                    mindist = dist;
                    result = p->pivot;
                }
            }
            if (--stackpos >= 0)
                ++state[stackpos];
        } else if (state[stackpos] == 0) {
            if (k->a[stackpos % 3] < p->pivot)
                stack[stackpos + 1] = p + 1;
            else
                stack[stackpos + 1] = p + p->offset;
            ++stackpos;
            state[stackpos] = 0;
        } else {
            int delta = k->a[stackpos % 3] - p->pivot;
            if (state[stackpos] == 1
                && (unsigned) (delta * delta) < mindist) {
                if (delta < 0)
                    stack[stackpos + 1] = p + p->offset;
                else
                    stack[stackpos + 1] = p + 1;
                ++stackpos;
                state[stackpos] = 0;
            } else if (--stackpos >= 0)
                ++state[stackpos];
        }
    }

    return result;
}

int kd3_closest(const kd3_tree* kd3, kd3_color k) {
    if (kd3->transform)
        kd3->transform(&k);
    return kd3_closest_transformed(kd3, &k);
}

int kd3_closest8(const kd3_tree* kd3, int a0, int a1, int a2) {
    kd3_color k;
    kd3_set8(&k, a0, a1, a2);
    if (kd3->transform)
        kd3->transform(&k);
    return kd3_closest_transformed(kd3, &k);
}


void
colormap_image_posterize(Gif_Image *gfi, uint8_t *new_data,
			 Gif_Colormap *old_cm, kd3_tree* kd3,
			 uint32_t *histogram)
{
  int ncol = old_cm->ncol;
  Gif_Color *col = old_cm->col;
  int map[256];
  int i, j;
  int transparent = gfi->transparent;

  /* find closest colors in new colormap */
  for (i = 0; i < ncol; i++) {
      map[i] = col[i].pixel = kd3_closest8(kd3, col[i].gfc_red, col[i].gfc_green, col[i].gfc_blue);
      col[i].haspixel = 1;
  }

  /* map image */
  for (j = 0; j < gfi->height; j++) {
    uint8_t *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++, new_data++)
      if (*data != transparent) {
	*new_data = map[*data];
	histogram[*new_data]++;
      }
  }
}


#define DITHER_SCALE	1024
#define DITHER_SHIFT    10
#define DITHER_SCALE_M1	(DITHER_SCALE-1)
#define DITHER_ITEM2ERR (1<<(DITHER_SHIFT-7))
#define N_RANDOM_VALUES	512

void
colormap_image_floyd_steinberg(Gif_Image *gfi, uint8_t *all_new_data,
			       Gif_Colormap *old_cm, kd3_tree* kd3,
                               uint32_t *histogram)
{
  static int32_t *random_values = 0;

  int width = gfi->width;
  int dither_direction = 0;
  int transparent = gfi->transparent;
  int i, j, k;
  kd3_color *err, *err1;

  /* Initialize distances */
  for (i = 0; i < old_cm->ncol; ++i) {
      Gif_Color* c = &old_cm->col[i];
      c->pixel = kd3_closest8(kd3, c->gfc_red, c->gfc_green, c->gfc_blue);
      c->haspixel = 1;
  }

  /* This code was written with reference to ppmquant by Jef Poskanzer, part
     of the pbmplus package. */

  /* Initialize Floyd-Steinberg error vectors to small random values, so we
     don't get artifacts on the top row */
  err = Gif_NewArray(kd3_color, width + 2);
  err1 = Gif_NewArray(kd3_color, width + 2);
  /* Use the same random values on each call in an attempt to minimize
     "jumping dithering" effects on animations */
  if (!random_values) {
    random_values = Gif_NewArray(int32_t, N_RANDOM_VALUES);
    for (i = 0; i < N_RANDOM_VALUES; i++)
      random_values[i] = RANDOM() % (DITHER_SCALE_M1 * 2) - DITHER_SCALE_M1;
  }
  for (i = 0; i < gfi->width + 2; i++) {
    j = (i + gfi->left) * 3;
    for (k = 0; k < 3; ++k)
        err[i].a[k] = random_values[ (j + k) % N_RANDOM_VALUES ];
  }
  /* err1 initialized below */

  /* Do the image! */
  for (j = 0; j < gfi->height; j++) {
    int d0, d1, d2, d3;		/* used for error diffusion */
    uint8_t *data, *new_data;
    int x;

    if (dither_direction) {
      x = width - 1;
      d0 = 0, d1 = 2, d2 = 1, d3 = 0;
    } else {
      x = 0;
      d0 = 2, d1 = 0, d2 = 1, d3 = 2;
    }
    data = &gfi->img[j][x];
    new_data = all_new_data + j * width + x;

    for (i = 0; i < width + 2; i++)
      err1[i].a[0] = err1[i].a[1] = err1[i].a[2] = 0;

    /* Do a single row */
    while (x >= 0 && x < width) {
      int e;
      kd3_color use;

      /* the transparent color never gets adjusted */
      if (*data == transparent)
	goto next;

      /* find desired new color */
      kd3_set8(&use, old_cm->col[*data].gfc_red, old_cm->col[*data].gfc_green,
               old_cm->col[*data].gfc_blue);
      if (kd3->transform)
          kd3->transform(&use);
      /* use Floyd-Steinberg errors to adjust */
      for (k = 0; k < 3; ++k) {
          use.a[k] += (err[x+1].a[k] & ~(DITHER_ITEM2ERR-1)) / DITHER_ITEM2ERR;
          use.a[k] = max(use.a[k], 0);
          use.a[k] = min(use.a[k], (255 << 7) | (255 >> 1));
      }

      e = old_cm->col[*data].pixel;
      if (kd3->distance(&kd3->items[e].k, &use) < kd3->xradius[e])
          *new_data = e;
      else
          *new_data = kd3_closest_transformed(kd3, &use);
      histogram[*new_data]++;

      /* calculate and propagate the error between desired and selected color.
	 Assume that, with a large scale (1024), we don't need to worry about
	 image artifacts caused by error accumulation (the fact that the
	 error terms might not sum to the error). */
      for (k = 0; k < 3; ++k) {
          e = (use.a[k] - kd3->items[*new_data].k.a[k]) * DITHER_ITEM2ERR;
          if (e) {
              err [x+d0].a[k] += ((e * 7) & ~15) / 16;
              err1[x+d1].a[k] += ((e * 3) & ~15) / 16;
              err1[x+d2].a[k] += ((e * 5) & ~15) / 16;
              err1[x+d3].a[k] += ( e      & ~15) / 16;
          }
      }

     next:
      if (dither_direction)
	x--, data--, new_data--;
      else
	x++, data++, new_data++;
    }
    /* Did a single row */

    /* change dithering directions */
    {
      kd3_color *temp = err1;
      err1 = err;
      err = temp;
      dither_direction = !dither_direction;
    }
  }

  /* delete temporary storage */
  Gif_DeleteArray(err);
  Gif_DeleteArray(err1);
}


/* return value 1 means run the image_changer again */
static int
try_assign_transparency(Gif_Image *gfi, Gif_Colormap *old_cm, uint8_t *new_data,
			Gif_Colormap *new_cm, int *new_ncol,
			kd3_tree* kd3, uint32_t *histogram)
{
  uint32_t min_used;
  int i, j;
  int transparent = gfi->transparent;
  int new_transparent = -1;
  Gif_Color transp_value;

  if (transparent < 0)
    return 0;

  if (old_cm)
    transp_value = old_cm->col[transparent];

  /* look for an unused pixel in the existing colormap; prefer the same color
     we had */
  for (i = 0; i < *new_ncol; i++)
    if (histogram[i] == 0 && GIF_COLOREQ(&transp_value, &new_cm->col[i])) {
      new_transparent = i;
      goto found;
    }
  for (i = 0; i < *new_ncol; i++)
    if (histogram[i] == 0) {
      new_transparent = i;
      goto found;
    }

  /* try to expand the colormap */
  if (*new_ncol < 256) {
    assert(*new_ncol < new_cm->capacity);
    new_transparent = *new_ncol;
    new_cm->col[new_transparent] = transp_value;
    (*new_ncol)++;
    goto found;
  }

  /* not found: mark the least-frequently-used color as the new transparent
     color and return 1 (meaning 'dither again') */
  assert(*new_ncol == 256);
  min_used = 0xFFFFFFFFU;
  for (i = 0; i < 256; i++)
    if (histogram[i] < min_used) {
      new_transparent = i;
      min_used = histogram[i];
    }
  kd3_disable(kd3, new_transparent); /* mark it as unusable */
  return 1;

 found:
  for (j = 0; j < gfi->height; j++) {
    uint8_t *data = gfi->img[j];
    for (i = 0; i < gfi->width; i++, data++, new_data++)
      if (*data == transparent)
	*new_data = new_transparent;
  }

  gfi->transparent = new_transparent;
  return 0;
}

void
colormap_stream(Gif_Stream *gfs, Gif_Colormap *new_cm,
		colormap_image_func image_changer)
{
  kd3_tree kd3;
  int background_transparent = gfs->images[0]->transparent >= 0;
  Gif_Color *new_col = new_cm->col;
  int new_ncol = new_cm->ncol, new_gray;
  int imagei, j;
  int compress_new_cm = 1;

  /* make sure colormap has enough space */
  if (new_cm->capacity < 256) {
    Gif_Color *x = Gif_NewArray(Gif_Color, 256);
    memcpy(x, new_col, sizeof(Gif_Color) * new_ncol);
    Gif_DeleteArray(new_col);
    new_cm->col = new_col = x;
    new_cm->capacity = 256;
  }
  assert(new_cm->capacity >= 256);

  /* new_col[j].pixel == number of pixels with color j in the new image. */
  for (j = 0; j < 256; j++)
    new_col[j].pixel = 0;

  /* initialize kd3 tree */
  new_gray = 1;
  for (j = 0; new_gray && j < new_cm->ncol; ++j)
      if (new_col[j].gfc_red != new_col[j].gfc_green
          || new_col[j].gfc_red != new_col[j].gfc_blue)
          new_gray = 0;
  if (new_gray)
      kd3_init(&kd3, kd3_distance, kd3_luminance_gamma_transform);
  else
      kd3_init(&kd3, kd3_distance, kd3_gamma_transform);
  for (j = 0; j < new_cm->ncol; ++j)
      kd3_add8(&kd3, new_col[j].gfc_red, new_col[j].gfc_green, new_col[j].gfc_blue);
  kd3_build(&kd3);

  for (imagei = 0; imagei < gfs->nimages; imagei++) {
    Gif_Image *gfi = gfs->images[imagei];
    Gif_Colormap *gfcm = gfi->local ? gfi->local : gfs->global;
    int only_compressed = (gfi->img == 0);

    if (gfcm) {
      /* If there was an old colormap, change the image data */
      uint8_t *new_data = Gif_NewArray(uint8_t, gfi->width * gfi->height);
      uint32_t histogram[256];
      unmark_colors(new_cm);
      unmark_colors(gfcm);

      if (only_compressed)
	Gif_UncompressImage(gfi);

      kd3_enable_all(&kd3);
      do {
	for (j = 0; j < 256; j++)
            histogram[j] = 0;
	image_changer(gfi, new_data, gfcm, &kd3, histogram);
      } while (try_assign_transparency(gfi, gfcm, new_data, new_cm, &new_ncol,
				       &kd3, histogram));

      Gif_ReleaseUncompressedImage(gfi);
      /* version 1.28 bug fix: release any compressed version or it'll cause
         bad images */
      Gif_ReleaseCompressedImage(gfi);
      Gif_SetUncompressedImage(gfi, new_data, Gif_DeleteArrayFunc, 0);

      if (only_compressed) {
	Gif_FullCompressImage(gfs, gfi, &gif_write_info);
	Gif_ReleaseUncompressedImage(gfi);
      }

      /* update count of used colors */
      for (j = 0; j < 256; j++)
	new_col[j].pixel += histogram[j];
      if (gfi->transparent >= 0)
	/* we don't have data on the number of used colors for transparency
	   so fudge it. */
	new_col[gfi->transparent].pixel += gfi->width * gfi->height / 8;

    } else {
      /* Can't compress new_cm afterwards if we didn't actively change colors
	 over */
      compress_new_cm = 0;
    }

    if (gfi->local) {
      Gif_DeleteColormap(gfi->local);
      gfi->local = 0;
    }
  }

  /* Set new_cm->ncol from new_ncol. We didn't update new_cm->ncol before so
     the closest-color algorithms wouldn't see any new transparent colors.
     That way added transparent colors were only used for transparency. */
  new_cm->ncol = new_ncol;

  /* change the background. I hate the background by now */
  if (background_transparent)
    gfs->background = gfs->images[0]->transparent;
  else if (gfs->global && gfs->background < gfs->global->ncol) {
    Gif_Color *c = &gfs->global->col[gfs->background];
    gfs->background = kd3_closest8(&kd3, c->gfc_red, c->gfc_green, c->gfc_blue);
    new_col[gfs->background].pixel++;
  }

  Gif_DeleteColormap(gfs->global);
  kd3_cleanup(&kd3);

  /* We may have used only a subset of the colors in new_cm. We try to store
     only that subset, just as if we'd piped the output of 'gifsicle
     --use-colormap=X' through 'gifsicle' another time. */
  gfs->global = Gif_CopyColormap(new_cm);
  for (j = 0; j < new_cm->ncol; ++j)
      gfs->global->col[j].haspixel = 0;
  if (compress_new_cm) {
    /* only bother to recompress if we'll get anything out of it */
    compress_new_cm = 0;
    for (j = 0; j < new_cm->ncol - 1; j++)
      if (new_col[j].pixel == 0 || new_col[j].pixel < new_col[j+1].pixel) {
	compress_new_cm = 1;
	break;
      }
  }

  if (compress_new_cm) {
    int map[256];

    /* Gif_CopyColormap copies the 'pixel' values as well */
    new_col = gfs->global->col;
    for (j = 0; j < new_cm->ncol; j++)
      new_col[j].haspixel = j;

    /* sort based on popularity */
    qsort(new_col, new_cm->ncol, sizeof(Gif_Color), popularity_sort_compare);

    /* set up the map and reduce the number of colors */
    for (j = 0; j < new_cm->ncol; j++)
      map[ new_col[j].haspixel ] = j;
    for (j = 0; j < new_cm->ncol; j++)
      if (!new_col[j].pixel) {
	gfs->global->ncol = j;
	break;
      }

    /* map the image data, transparencies, and background */
    gfs->background = map[gfs->background];
    for (imagei = 0; imagei < gfs->nimages; imagei++) {
      Gif_Image *gfi = gfs->images[imagei];
      int only_compressed = (gfi->img == 0);
      uint32_t size;
      uint8_t *data;
      if (only_compressed)
	Gif_UncompressImage(gfi);

      data = gfi->image_data;
      for (size = gfi->width * gfi->height; size > 0; size--, data++)
	*data = map[*data];
      if (gfi->transparent >= 0)
	gfi->transparent = map[gfi->transparent];

      if (only_compressed) {
	Gif_FullCompressImage(gfs, gfi, &gif_write_info);
	Gif_ReleaseUncompressedImage(gfi);
      }
    }
  }
}
