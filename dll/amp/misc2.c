/* this file is a part of amp software, (C) tomislav uzelac 1996,1997
*/
 
/* misc2.c  requantization, stereo processing, reordering(shortbl) and antialiasing butterflies
 *
 * misc.c was created by tomislav uzelac in May 1996, and was completely awful
 * Created by: tomislav uzelac Dec 22 1996
 * some more speed injected, cca. Jun 1 1997
 */
#include <math.h>

#include "amp.h"
#include "audio.h"
#include "getdata.h"
#include "huffman.h"

#define MISC2
#include "misc2.h"

int no_of_imdcts[2];

static const int t_pretab[22]={0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,3,3,3,2,0};
static const float t_is[7]={ 1.0, 0.788675134596, 0.633974596215,
		 0.5, 0.366025403784, 0.211324865405, 0.0};
static const float t_is2[2][32]={
{             1,       0.840896,       0.707107,       0.594604,            0.5,       0.420448,
       0.353553,       0.297302,           0.25,       0.210224,       0.176777,       0.148651,
          0.125,       0.105112,      0.0883883,      0.0743254},
{             1,       0.707107,            0.5,       0.353553,           0.25,       0.176777,
          0.125,      0.0883883,         0.0625,      0.0441942,        0.03125,      0.0220971,
       0.015625,      0.0110485,      0.0078125,     0.00552427}
};

static const float t_downmix[2][32]={
{ 1.000000,   0.920448,   0.853554,   0.797302,   0.750000,   0.710224,
  0.676776,   0.648651,   0.625000,   0.605112,   0.588389,   0.574326,
  0.562500,   0.552556,   0.544194,   0.537163},
{ 1.000000,   0.853554,   0.750000,   0.676776,   0.625000,   0.588389,
  0.562500,   0.544194,   0.531250,   0.522097,   0.515625,   0.511049,
  0.507813,   0.505524,   0.503906,   0.502762}
};

static const float Cs[8]={0.857492925712, 0.881741997318, 0.949628649103,
	      0.983314592492, 0.995517816065, 0.999160558175,
	      0.999899195243, 0.999993155067};
static const float Ca[8]={-0.5144957554270, -0.4717319685650, -0.3133774542040,
	      -0.1819131996110, -0.0945741925262, -0.0409655828852,
	      -0.0141985685725,	-0.00369997467375};
static const float tab[4]={1,1.189207115,1.414213562,1.6817928301};
static const float tabi[4]={1,0.840896415,0.707106781,0.594603557};

static float t_43[8192];


/* leftmost index denotes header->ID, so first three are for MPEG2
 * and the others are for MPEG1
 */
static const short t_reorder[2][3][576]={{
{  0,   1,   2,   3,   6,   7,   8,   9,  12,  13,  14,  15,   4,   5,  18,  19,  10,  11,  24,  25,
  16,  17,  30,  31,  20,  21,  22,  23,  26,  27,  28,  29,  32,  33,  34,  35,  36,  37,  38,  39,
  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  90,  91,
  78,  79,  80,  81,  82,  83,  96,  97,  84,  85,  86,  87,  88,  89, 102, 103,  92,  93,  94,  95,
 108, 109, 110, 111, 112, 113,  98,  99, 100, 101, 114, 115, 116, 117, 118, 119, 104, 105, 106, 107,
 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 144, 145, 146, 147, 148, 149, 162, 163,
 132, 133, 134, 135, 136, 137, 150, 151, 152, 153, 154, 155, 168, 169, 138, 139, 140, 141, 142, 143,
 156, 157, 158, 159, 160, 161, 174, 175, 164, 165, 166, 167, 180, 181, 182, 183, 184, 185, 198, 199,
 200, 201, 202, 203, 216, 217, 170, 171, 172, 173, 186, 187, 188, 189, 190, 191, 204, 205, 206, 207,
 208, 209, 222, 223, 176, 177, 178, 179, 192, 193, 194, 195, 196, 197, 210, 211, 212, 213, 214, 215,
 228, 229, 218, 219, 220, 221, 234, 235, 236, 237, 238, 239, 252, 253, 254, 255, 256, 257, 270, 271,
 272, 273, 274, 275, 288, 289, 290, 291, 224, 225, 226, 227, 240, 241, 242, 243, 244, 245, 258, 259,
 260, 261, 262, 263, 276, 277, 278, 279, 280, 281, 294, 295, 296, 297, 230, 231, 232, 233, 246, 247,
 248, 249, 250, 251, 264, 265, 266, 267, 268, 269, 282, 283, 284, 285, 286, 287, 300, 301, 302, 303,
 292, 293, 306, 307, 308, 309, 310, 311, 324, 325, 326, 327, 328, 329, 342, 343, 344, 345, 346, 347,
 360, 361, 362, 363, 364, 365, 378, 379, 380, 381, 382, 383, 298, 299, 312, 313, 314, 315, 316, 317,
 330, 331, 332, 333, 334, 335, 348, 349, 350, 351, 352, 353, 366, 367, 368, 369, 370, 371, 384, 385,
 386, 387, 388, 389, 304, 305, 318, 319, 320, 321, 322, 323, 336, 337, 338, 339, 340, 341, 354, 355,
 356, 357, 358, 359, 372, 373, 374, 375, 376, 377, 390, 391, 392, 393, 394, 395, 396, 397, 398, 399,
 400, 401, 414, 415, 416, 417, 418, 419, 432, 433, 434, 435, 436, 437, 450, 451, 452, 453, 454, 455,
 468, 469, 470, 471, 472, 473, 486, 487, 488, 489, 490, 491, 504, 505, 506, 507, 508, 509, 402, 403,
 404, 405, 406, 407, 420, 421, 422, 423, 424, 425, 438, 439, 440, 441, 442, 443, 456, 457, 458, 459,
 460, 461, 474, 475, 476, 477, 478, 479, 492, 493, 494, 495, 496, 497, 510, 511, 512, 513, 514, 515,
 408, 409, 410, 411, 412, 413, 426, 427, 428, 429, 430, 431, 444, 445, 446, 447, 448, 449, 462, 463,
 464, 465, 466, 467, 480, 481, 482, 483, 484, 485, 498, 499, 500, 501, 502, 503, 516, 517, 518, 519,
 520, 521, 522, 523, 524, 525, 526, 527, 540, 541, 542, 543, 544, 545, 558, 559, 560, 561, 562, 563,
 528, 529, 530, 531, 532, 533, 546, 547, 548, 549, 550, 551, 564, 565, 566, 567, 568, 569, 534, 535,
 536, 537, 538, 539, 552, 553, 554, 555, 556, 557, 570, 571, 572, 573, 574, 575},

{  0,   1,   2,   3,   6,   7,   8,   9,  12,  13,  14,  15,   4,   5,  18,  19,  10,  11,  24,  25,
  16,  17,  30,  31,  20,  21,  22,  23,  26,  27,  28,  29,  32,  33,  34,  35,  36,  37,  38,  39,
  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
  72,  73,  60,  61,  62,  63,  64,  65,  78,  79,  66,  67,  68,  69,  70,  71,  84,  85,  74,  75,
  76,  77,  90,  91,  92,  93,  94,  95,  80,  81,  82,  83,  96,  97,  98,  99, 100, 101,  86,  87,
  88,  89, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 126, 127, 128, 129, 130, 131,
 114, 115, 116, 117, 118, 119, 132, 133, 134, 135, 136, 137, 120, 121, 122, 123, 124, 125, 138, 139,
 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 162, 163, 164, 165, 166, 167, 180, 181, 150, 151,
 152, 153, 154, 155, 168, 169, 170, 171, 172, 173, 186, 187, 156, 157, 158, 159, 160, 161, 174, 175,
 176, 177, 178, 179, 192, 193, 182, 183, 184, 185, 198, 199, 200, 201, 202, 203, 216, 217, 218, 219,
 220, 221, 234, 235, 188, 189, 190, 191, 204, 205, 206, 207, 208, 209, 222, 223, 224, 225, 226, 227,
 240, 241, 194, 195, 196, 197, 210, 211, 212, 213, 214, 215, 228, 229, 230, 231, 232, 233, 246, 247,
 236, 237, 238, 239, 252, 253, 254, 255, 256, 257, 270, 271, 272, 273, 274, 275, 288, 289, 290, 291,
 292, 293, 306, 307, 242, 243, 244, 245, 258, 259, 260, 261, 262, 263, 276, 277, 278, 279, 280, 281,
 294, 295, 296, 297, 298, 299, 312, 313, 248, 249, 250, 251, 264, 265, 266, 267, 268, 269, 282, 283,
 284, 285, 286, 287, 300, 301, 302, 303, 304, 305, 318, 319, 308, 309, 310, 311, 324, 325, 326, 327,
 328, 329, 342, 343, 344, 345, 346, 347, 360, 361, 362, 363, 364, 365, 378, 379, 380, 381, 382, 383,
 396, 397, 398, 399, 314, 315, 316, 317, 330, 331, 332, 333, 334, 335, 348, 349, 350, 351, 352, 353,
 366, 367, 368, 369, 370, 371, 384, 385, 386, 387, 388, 389, 402, 403, 404, 405, 320, 321, 322, 323,
 336, 337, 338, 339, 340, 341, 354, 355, 356, 357, 358, 359, 372, 373, 374, 375, 376, 377, 390, 391,
 392, 393, 394, 395, 408, 409, 410, 411, 400, 401, 414, 415, 416, 417, 418, 419, 432, 433, 434, 435,
 436, 437, 450, 451, 452, 453, 454, 455, 468, 469, 470, 471, 472, 473, 486, 487, 488, 489, 490, 491,
 504, 505, 506, 507, 508, 509, 522, 523, 524, 525, 526, 527, 406, 407, 420, 421, 422, 423, 424, 425,
 438, 439, 440, 441, 442, 443, 456, 457, 458, 459, 460, 461, 474, 475, 476, 477, 478, 479, 492, 493,
 494, 495, 496, 497, 510, 511, 512, 513, 514, 515, 528, 529, 530, 531, 532, 533, 412, 413, 426, 427,
 428, 429, 430, 431, 444, 445, 446, 447, 448, 449, 462, 463, 464, 465, 466, 467, 480, 481, 482, 483,
 484, 485, 498, 499, 500, 501, 502, 503, 516, 517, 518, 519, 520, 521, 534, 535, 536, 537, 538, 539,
 540, 541, 542, 543, 544, 545, 558, 559, 560, 561, 562, 563, 546, 547, 548, 549, 550, 551, 564, 565,
 566, 567, 568, 569, 552, 553, 554, 555, 556, 557, 570, 571, 572, 573, 574, 575},

{  0,   1,   2,   3,   6,   7,   8,   9,  12,  13,  14,  15,   4,   5,  18,  19,  10,  11,  24,  25,
  16,  17,  30,  31,  20,  21,  22,  23,  26,  27,  28,  29,  32,  33,  34,  35,  36,  37,  38,  39,
  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
  72,  73,  60,  61,  62,  63,  64,  65,  78,  79,  66,  67,  68,  69,  70,  71,  84,  85,  74,  75,
  76,  77,  90,  91,  92,  93,  94,  95,  80,  81,  82,  83,  96,  97,  98,  99, 100, 101,  86,  87,
  88,  89, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 126, 127, 128, 129, 130, 131,
 114, 115, 116, 117, 118, 119, 132, 133, 134, 135, 136, 137, 120, 121, 122, 123, 124, 125, 138, 139,
 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 162, 163, 164, 165, 166, 167, 180, 181, 150, 151,
 152, 153, 154, 155, 168, 169, 170, 171, 172, 173, 186, 187, 156, 157, 158, 159, 160, 161, 174, 175,
 176, 177, 178, 179, 192, 193, 182, 183, 184, 185, 198, 199, 200, 201, 202, 203, 216, 217, 218, 219,
 220, 221, 234, 235, 188, 189, 190, 191, 204, 205, 206, 207, 208, 209, 222, 223, 224, 225, 226, 227,
 240, 241, 194, 195, 196, 197, 210, 211, 212, 213, 214, 215, 228, 229, 230, 231, 232, 233, 246, 247,
 236, 237, 238, 239, 252, 253, 254, 255, 256, 257, 270, 271, 272, 273, 274, 275, 288, 289, 290, 291,
 292, 293, 306, 307, 242, 243, 244, 245, 258, 259, 260, 261, 262, 263, 276, 277, 278, 279, 280, 281,
 294, 295, 296, 297, 298, 299, 312, 313, 248, 249, 250, 251, 264, 265, 266, 267, 268, 269, 282, 283,
 284, 285, 286, 287, 300, 301, 302, 303, 304, 305, 318, 319, 308, 309, 310, 311, 324, 325, 326, 327,
 328, 329, 342, 343, 344, 345, 346, 347, 360, 361, 362, 363, 364, 365, 378, 379, 380, 381, 382, 383,
 396, 397, 314, 315, 316, 317, 330, 331, 332, 333, 334, 335, 348, 349, 350, 351, 352, 353, 366, 367,
 368, 369, 370, 371, 384, 385, 386, 387, 388, 389, 402, 403, 320, 321, 322, 323, 336, 337, 338, 339,
 340, 341, 354, 355, 356, 357, 358, 359, 372, 373, 374, 375, 376, 377, 390, 391, 392, 393, 394, 395,
 408, 409, 398, 399, 400, 401, 414, 415, 416, 417, 418, 419, 432, 433, 434, 435, 436, 437, 450, 451,
 452, 453, 454, 455, 468, 469, 470, 471, 472, 473, 486, 487, 488, 489, 490, 491, 504, 505, 506, 507,
 508, 509, 404, 405, 406, 407, 420, 421, 422, 423, 424, 425, 438, 439, 440, 441, 442, 443, 456, 457,
 458, 459, 460, 461, 474, 475, 476, 477, 478, 479, 492, 493, 494, 495, 496, 497, 510, 511, 512, 513,
 514, 515, 410, 411, 412, 413, 426, 427, 428, 429, 430, 431, 444, 445, 446, 447, 448, 449, 462, 463,
 464, 465, 466, 467, 480, 481, 482, 483, 484, 485, 498, 499, 500, 501, 502, 503, 516, 517, 518, 519,
 520, 521, 522, 523, 524, 525, 526, 527, 540, 541, 542, 543, 544, 545, 558, 559, 560, 561, 562, 563,
 528, 529, 530, 531, 532, 533, 546, 547, 548, 549, 550, 551, 564, 565, 566, 567, 568, 569, 534, 535,
 536, 537, 538, 539, 552, 553, 554, 555, 556, 557, 570, 571, 572, 573, 574, 575}
},
{
{  0,   1,   2,   3,   6,   7,   8,   9,  12,  13,  14,  15,   4,   5,  18,  19,  10,  11,  24,  25,
  16,  17,  30,  31,  20,  21,  22,  23,  26,  27,  28,  29,  32,  33,  34,  35,  36,  37,  38,  39,
  42,  43,  44,  45,  48,  49,  50,  51,  40,  41,  54,  55,  56,  57,  46,  47,  60,  61,  62,  63,
  52,  53,  66,  67,  68,  69,  58,  59,  72,  73,  74,  75,  76,  77,  64,  65,  78,  79,  80,  81,
  82,  83,  70,  71,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95, 108, 109, 110, 111,
  96,  97,  98,  99, 100, 101, 114, 115, 116, 117, 102, 103, 104, 105, 106, 107, 120, 121, 122, 123,
 112, 113, 126, 127, 128, 129, 130, 131, 144, 145, 146, 147, 118, 119, 132, 133, 134, 135, 136, 137,
 150, 151, 152, 153, 124, 125, 138, 139, 140, 141, 142, 143, 156, 157, 158, 159, 148, 149, 162, 163,
 164, 165, 166, 167, 180, 181, 182, 183, 184, 185, 154, 155, 168, 169, 170, 171, 172, 173, 186, 187,
 188, 189, 190, 191, 160, 161, 174, 175, 176, 177, 178, 179, 192, 193, 194, 195, 196, 197, 198, 199,
 200, 201, 202, 203, 216, 217, 218, 219, 220, 221, 234, 235, 236, 237, 238, 239, 204, 205, 206, 207,
 208, 209, 222, 223, 224, 225, 226, 227, 240, 241, 242, 243, 244, 245, 210, 211, 212, 213, 214, 215,
 228, 229, 230, 231, 232, 233, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255, 256, 257, 270, 271,
 272, 273, 274, 275, 288, 289, 290, 291, 292, 293, 306, 307, 308, 309, 258, 259, 260, 261, 262, 263,
 276, 277, 278, 279, 280, 281, 294, 295, 296, 297, 298, 299, 312, 313, 314, 315, 264, 265, 266, 267,
 268, 269, 282, 283, 284, 285, 286, 287, 300, 301, 302, 303, 304, 305, 318, 319, 320, 321, 310, 311,
 324, 325, 326, 327, 328, 329, 342, 343, 344, 345, 346, 347, 360, 361, 362, 363, 364, 365, 378, 379,
 380, 381, 382, 383, 396, 397, 398, 399, 316, 317, 330, 331, 332, 333, 334, 335, 348, 349, 350, 351,
 352, 353, 366, 367, 368, 369, 370, 371, 384, 385, 386, 387, 388, 389, 402, 403, 404, 405, 322, 323,
 336, 337, 338, 339, 340, 341, 354, 355, 356, 357, 358, 359, 372, 373, 374, 375, 376, 377, 390, 391,
 392, 393, 394, 395, 408, 409, 410, 411, 400, 401, 414, 415, 416, 417, 418, 419, 432, 433, 434, 435,
 436, 437, 450, 451, 452, 453, 454, 455, 468, 469, 470, 471, 472, 473, 486, 487, 488, 489, 490, 491,
 504, 505, 506, 507, 508, 509, 522, 523, 524, 525, 526, 527, 540, 541, 542, 543, 544, 545, 558, 559,
 560, 561, 562, 563, 406, 407, 420, 421, 422, 423, 424, 425, 438, 439, 440, 441, 442, 443, 456, 457,
 458, 459, 460, 461, 474, 475, 476, 477, 478, 479, 492, 493, 494, 495, 496, 497, 510, 511, 512, 513,
 514, 515, 528, 529, 530, 531, 532, 533, 546, 547, 548, 549, 550, 551, 564, 565, 566, 567, 568, 569,
 412, 413, 426, 427, 428, 429, 430, 431, 444, 445, 446, 447, 448, 449, 462, 463, 464, 465, 466, 467,
 480, 481, 482, 483, 484, 485, 498, 499, 500, 501, 502, 503, 516, 517, 518, 519, 520, 521, 534, 535,
 536, 537, 538, 539, 552, 553, 554, 555, 556, 557, 570, 571, 572, 573, 574, 575},

{  0,   1,   2,   3,   6,   7,   8,   9,  12,  13,  14,  15,   4,   5,  18,  19,  10,  11,  24,  25,
  16,  17,  30,  31,  20,  21,  22,  23,  26,  27,  28,  29,  32,  33,  34,  35,  36,  37,  38,  39,
  42,  43,  44,  45,  48,  49,  50,  51,  40,  41,  54,  55,  56,  57,  46,  47,  60,  61,  62,  63,
  52,  53,  66,  67,  68,  69,  58,  59,  72,  73,  74,  75,  64,  65,  78,  79,  80,  81,  70,  71,
  84,  85,  86,  87,  76,  77,  90,  91,  92,  93,  94,  95, 108, 109,  82,  83,  96,  97,  98,  99,
 100, 101, 114, 115,  88,  89, 102, 103, 104, 105, 106, 107, 120, 121, 110, 111, 112, 113, 126, 127,
 128, 129, 130, 131, 144, 145, 116, 117, 118, 119, 132, 133, 134, 135, 136, 137, 150, 151, 122, 123,
 124, 125, 138, 139, 140, 141, 142, 143, 156, 157, 146, 147, 148, 149, 162, 163, 164, 165, 166, 167,
 180, 181, 182, 183, 152, 153, 154, 155, 168, 169, 170, 171, 172, 173, 186, 187, 188, 189, 158, 159,
 160, 161, 174, 175, 176, 177, 178, 179, 192, 193, 194, 195, 184, 185, 198, 199, 200, 201, 202, 203,
 216, 217, 218, 219, 220, 221, 234, 235, 190, 191, 204, 205, 206, 207, 208, 209, 222, 223, 224, 225,
 226, 227, 240, 241, 196, 197, 210, 211, 212, 213, 214, 215, 228, 229, 230, 231, 232, 233, 246, 247,
 236, 237, 238, 239, 252, 253, 254, 255, 256, 257, 270, 271, 272, 273, 274, 275, 288, 289, 290, 291,
 242, 243, 244, 245, 258, 259, 260, 261, 262, 263, 276, 277, 278, 279, 280, 281, 294, 295, 296, 297,
 248, 249, 250, 251, 264, 265, 266, 267, 268, 269, 282, 283, 284, 285, 286, 287, 300, 301, 302, 303,
 292, 293, 306, 307, 308, 309, 310, 311, 324, 325, 326, 327, 328, 329, 342, 343, 344, 345, 346, 347,
 360, 361, 362, 363, 364, 365, 298, 299, 312, 313, 314, 315, 316, 317, 330, 331, 332, 333, 334, 335,
 348, 349, 350, 351, 352, 353, 366, 367, 368, 369, 370, 371, 304, 305, 318, 319, 320, 321, 322, 323,
 336, 337, 338, 339, 340, 341, 354, 355, 356, 357, 358, 359, 372, 373, 374, 375, 376, 377, 378, 379,
 380, 381, 382, 383, 396, 397, 398, 399, 400, 401, 414, 415, 416, 417, 418, 419, 432, 433, 434, 435,
 436, 437, 450, 451, 452, 453, 454, 455, 468, 469, 470, 471, 472, 473, 486, 487, 488, 489, 490, 491,
 504, 505, 506, 507, 508, 509, 522, 523, 524, 525, 526, 527, 540, 541, 542, 543, 544, 545, 558, 559,
 560, 561, 562, 563, 384, 385, 386, 387, 388, 389, 402, 403, 404, 405, 406, 407, 420, 421, 422, 423,
 424, 425, 438, 439, 440, 441, 442, 443, 456, 457, 458, 459, 460, 461, 474, 475, 476, 477, 478, 479,
 492, 493, 494, 495, 496, 497, 510, 511, 512, 513, 514, 515, 528, 529, 530, 531, 532, 533, 546, 547,
 548, 549, 550, 551, 564, 565, 566, 567, 568, 569, 390, 391, 392, 393, 394, 395, 408, 409, 410, 411,
 412, 413, 426, 427, 428, 429, 430, 431, 444, 445, 446, 447, 448, 449, 462, 463, 464, 465, 466, 467,
 480, 481, 482, 483, 484, 485, 498, 499, 500, 501, 502, 503, 516, 517, 518, 519, 520, 521, 534, 535,
 536, 537, 538, 539, 552, 553, 554, 555, 556, 557, 570, 571, 572, 573, 574, 575},

{  0,   1,   2,   3,   6,   7,   8,   9,  12,  13,  14,  15,   4,   5,  18,  19,  10,  11,  24,  25,
  16,  17,  30,  31,  20,  21,  22,  23,  26,  27,  28,  29,  32,  33,  34,  35,  36,  37,  38,  39,
  42,  43,  44,  45,  48,  49,  50,  51,  40,  41,  54,  55,  56,  57,  46,  47,  60,  61,  62,  63,
  52,  53,  66,  67,  68,  69,  58,  59,  72,  73,  74,  75,  76,  77,  64,  65,  78,  79,  80,  81,
  82,  83,  70,  71,  84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95, 108, 109, 110, 111,
 112, 113,  96,  97,  98,  99, 100, 101, 114, 115, 116, 117, 118, 119, 102, 103, 104, 105, 106, 107,
 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 144, 145, 146, 147, 148, 149, 162, 163,
 164, 165, 132, 133, 134, 135, 136, 137, 150, 151, 152, 153, 154, 155, 168, 169, 170, 171, 138, 139,
 140, 141, 142, 143, 156, 157, 158, 159, 160, 161, 174, 175, 176, 177, 166, 167, 180, 181, 182, 183,
 184, 185, 198, 199, 200, 201, 202, 203, 216, 217, 218, 219, 220, 221, 172, 173, 186, 187, 188, 189,
 190, 191, 204, 205, 206, 207, 208, 209, 222, 223, 224, 225, 226, 227, 178, 179, 192, 193, 194, 195,
 196, 197, 210, 211, 212, 213, 214, 215, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
 252, 253, 254, 255, 256, 257, 270, 271, 272, 273, 274, 275, 288, 289, 290, 291, 292, 293, 306, 307,
 240, 241, 242, 243, 244, 245, 258, 259, 260, 261, 262, 263, 276, 277, 278, 279, 280, 281, 294, 295,
 296, 297, 298, 299, 312, 313, 246, 247, 248, 249, 250, 251, 264, 265, 266, 267, 268, 269, 282, 283,
 284, 285, 286, 287, 300, 301, 302, 303, 304, 305, 318, 319, 308, 309, 310, 311, 324, 325, 326, 327,
 328, 329, 342, 343, 344, 345, 346, 347, 360, 361, 362, 363, 364, 365, 378, 379, 380, 381, 382, 383,
 396, 397, 398, 399, 400, 401, 314, 315, 316, 317, 330, 331, 332, 333, 334, 335, 348, 349, 350, 351,
 352, 353, 366, 367, 368, 369, 370, 371, 384, 385, 386, 387, 388, 389, 402, 403, 404, 405, 406, 407,
 320, 321, 322, 323, 336, 337, 338, 339, 340, 341, 354, 355, 356, 357, 358, 359, 372, 373, 374, 375,
 376, 377, 390, 391, 392, 393, 394, 395, 408, 409, 410, 411, 412, 413, 414, 415, 416, 417, 418, 419,
 432, 433, 434, 435, 436, 437, 450, 451, 452, 453, 454, 455, 468, 469, 470, 471, 472, 473, 486, 487,
 488, 489, 490, 491, 504, 505, 506, 507, 508, 509, 522, 523, 524, 525, 526, 527, 420, 421, 422, 423,
 424, 425, 438, 439, 440, 441, 442, 443, 456, 457, 458, 459, 460, 461, 474, 475, 476, 477, 478, 479,
 492, 493, 494, 495, 496, 497, 510, 511, 512, 513, 514, 515, 528, 529, 530, 531, 532, 533, 426, 427,
 428, 429, 430, 431, 444, 445, 446, 447, 448, 449, 462, 463, 464, 465, 466, 467, 480, 481, 482, 483,
 484, 485, 498, 499, 500, 501, 502, 503, 516, 517, 518, 519, 520, 521, 534, 535, 536, 537, 538, 539,
 540, 541, 542, 543, 544, 545, 558, 559, 560, 561, 562, 563, 546, 547, 548, 549, 550, 551, 564, 565,
 566, 567, 568, 569, 552, 553, 554, 555, 556, 557, 570, 571, 572, 573, 574, 575}
}};

/* 
 * fras == Formula for Requantization and All Scaling **************************
 */
static inline float fras_l(int sfb,int global_gain,int scalefac_scale,int scalefac,int preflag)
{
register int a,scale;
	/*
        if (scalefac_scale) scale=2;
	else scale=1;
	*/
	scale=scalefac_scale+1;
	a= global_gain -210 -(scalefac << scale);
        if (preflag) a-=(t_pretab[sfb] << scale);

/* bugfix, Mar 13 97: shifting won't produce a legal result if we shift by more than 31
 * since global_gain<256, this can only occur for (very) negative values of a.
*/
	if (a < -127) return 0;

/* a minor change here as well, no point in abs() if we now that a<0
*/
        if (a>=0) return tab[a&3]*(1 << (a>>2));
	else return tabi[(-a)&3]/(1 << ((-a) >> 2));
}

static inline float fras_s(int global_gain,int subblock_gain,int scalefac_scale,int scalefac)
{
int a;
        a=global_gain - 210 - (subblock_gain << 3);
	if (scalefac_scale) a-= (scalefac << 2);
	else a-= (scalefac << 1);

        if (a < -127) return 0;

        if (a>=0) return tab[a&3]*(1 << (a>>2));
        else return tabi[(-a)&3]/(1 << ((-a) >> 2));
}

/* this should be faster than pow()
 */
static inline float fras2(int is,float a)
{
        if (is > 0) return t_43[is]*a;
        else return -t_43[-is]*a;
}

/*
 * requantize_mono *************************************************************
 */

/* generally, the two channels do not have to be of the same block type - that's why we do two passes with requantize_mono.
 * if ms or intensity stereo is enabled we do a single pass with requantize_ms because both channels are same block type
 */

void requantize_mono(int gr,int ch,struct SIDE_INFO *info,struct AUDIO_HEADER *header)
{
int l,i,sfb;
float a;
int global_gain=info->global_gain[gr][ch];
int scalefac_scale=info->scalefac_scale[gr][ch];
int sfreq=header->sampling_frequency;


	no_of_imdcts[0]=no_of_imdcts[1]=32;

	if (info->window_switching_flag[gr][ch] && info->block_type[gr][ch]==2)	
		if (info->mixed_block_flag[gr][ch]) {
	/*
	 * requantize_mono - mixed blocks/long block part **********************
	 */
	                int window,window_len,preflag=0; /* pretab is all zero in this low frequency area */
	                int scalefac=scalefac_l[gr][ch][0];

			l=0;sfb=0;
			a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
	                while (l<36) {
	                        xr[ch][0][l]=fras2(is[ch][l],a);
	                        if (l==t_l[sfb]) {
					scalefac=scalefac_l[gr][ch][++sfb];
					a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
				}
	                        l++;
	                }
	/*
	 * requantize_mono - mixed blocks/short block part *********************
	 */
			sfb=3; 
			window_len=t_s[sfb]-t_s[sfb-1];
	                while (l<non_zero[ch]) {
	                	for (window=0;window<3;window++) {
	                        	int scalefac=scalefac_s[gr][ch][sfb][window];
	                                int subblock_gain=info->subblock_gain[gr][ch][window];
					a=fras_s(global_gain,subblock_gain,scalefac_scale,scalefac);
	                                for (i=0;i<window_len;i++) {
      		                                xr[ch][0][t_reorder[header->ID][sfreq][l]]=fras2(is[ch][l],a);
	                                	l++;
                                       	}
                               	}
                               	sfb++;
                               	window_len=t_s[sfb]-t_s[sfb-1];
			}
			while (l<576) xr[ch][0][t_reorder[header->ID][sfreq][l++]]=0;
		} else {
	/*
	 * requantize mono - short blocks **************************************
	 */
			int window,window_len;

			sfb=0; l=0;
			window_len=t_s[0]+1;
			while (l<non_zero[ch]) {
				for (window=0;window<3;window++) {
					int scalefac=scalefac_s[gr][ch][sfb][window];
					int subblock_gain=info->subblock_gain[gr][ch][window];
					float a=fras_s(global_gain,subblock_gain,scalefac_scale,scalefac);
					for (i=0;i<window_len;i++) {
						xr[ch][0][t_reorder[header->ID][sfreq][l]]=fras2(is[ch][l],a);
						l++;
					}
				}
				sfb++;
				window_len=t_s[sfb]-t_s[sfb-1];
			}
			while (l<576) xr[ch][0][t_reorder[header->ID][sfreq][l++]]=0;
		}
	else {
	/* long blocks */
		int preflag=info->preflag[gr][ch];
		int scalefac=scalefac_l[gr][ch][0];

		sfb=0; l=0;
		a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
		while (l<non_zero[ch]) {
			xr[ch][0][l]=fras2(is[ch][l],a); 
			if (l==t_l[sfb]) {
				scalefac=scalefac_l[gr][ch][++sfb];
				a=fras_l(sfb,global_gain,scalefac_scale,scalefac,preflag);
			}
			l++;
		}		
		while (l<576) xr[ch][0][l++]=0;
	}
}

/*
 * stereo stuff ****************************************************************
 */
static int find_isbound(int isbound[3],int gr,struct SIDE_INFO *info,struct AUDIO_HEADER *header)
{
int sfb,window,window_len,ms_flag,tmp,i;

	isbound[0]=isbound[1]=isbound[2]=-1;
	no_of_imdcts[0]=no_of_imdcts[1]=32;

   if (header->mode_extension==1 || header->mode_extension==3) {
	if (info->window_switching_flag[gr][0] && info->block_type[gr][0]==2) {

	/* find that isbound!
	 */
		tmp=non_zero[1];
		sfb=0; while ((3*t_s[sfb]+2) < tmp  && sfb < 12) sfb++;
		while ((isbound[0]<0 || isbound[1]<0 || isbound[2]<0) && !(info->mixed_block_flag[gr][0] && sfb<3) && sfb) {
			for (window=0;window<3;window++) {
				if (sfb==0) {
					window_len=t_s[0]+1;
					tmp=(window+1)*window_len - 1;
				} else {
					window_len=t_s[sfb]-t_s[sfb-1];
					tmp=(3*t_s[sfb-1]+2) + (window+1)*window_len;
				}
				if (isbound[window] < 0)
					for (i=0;i<window_len;i++)
						if (is[1][tmp--] != 0) {
							isbound[window]=t_s[sfb]+1; 
							break;
						}
			}
			sfb--;
		}
	
	/* mixed block magic now...
	 */
		if (sfb==2 && info->mixed_block_flag[gr][0])
		{ 
			if (isbound[0]<0 && isbound[1]<0 && isbound[2]<0) 
			{
				tmp=35;
				while (is[1][tmp] == 0) tmp--;
				sfb=0; while (t_l[sfb] < tmp  && sfb < 21) sfb++;
				isbound[0]=isbound[1]=isbound[2]=t_l[sfb]+1;
			} else for (window=0;window<3;window++) 
				if (isbound[window]<0) isbound[window]=36;
		}
		if (header->ID==1) isbound[0]=isbound[1]=isbound[2]=MAX(isbound[0],MAX(isbound[1],isbound[2]));

	/* just how many imdcts?
	 */
		tmp=non_zero[0];
		sfb=0; while ((3*t_s[sfb]+2) < tmp && sfb < 12) sfb++;
		no_of_imdcts[0]=no_of_imdcts[1]=(t_s[sfb]-1)/6+1; 
	} else {

	/* long blocks now
	 */
                tmp=non_zero[1];
                while (is[1][tmp] == 0) tmp--;
                sfb=0; while (t_l[sfb] < tmp && sfb < 21) sfb++;
		isbound[0]=isbound[1]=isbound[2]=t_l[sfb]+1;
		no_of_imdcts[0]=no_of_imdcts[1]=(non_zero[0]-1)/18+1; 
	}
	if (header->mode_extension==1) ms_flag=0;
	else ms_flag=1;
   } else {

   /* intensity stereo is, regretably, turned off
    */
	ms_flag=1;

	/* i really put a lot of work in this, but it still looks like shit (works, though)
	 */ 
	if (!info->window_switching_flag[gr][0] || (info->window_switching_flag[gr][0] && info->block_type[gr][0]!=2)) 
		isbound[0]=isbound[1]=isbound[2]=(MAX(non_zero[0],non_zero[1]));
	else isbound[0]=isbound[1]=isbound[2]=576;

	if (info->window_switching_flag[gr][0] && info->block_type[gr][0]==2) {
		/* should do for mixed blocks too, though i havent tested... */
			tmp=(MAX(non_zero[0],non_zero[1]))/3;
			sfb=0; while (t_s[sfb]<tmp && sfb<12) sfb++;
			no_of_imdcts[0]=no_of_imdcts[1]=(t_s[sfb]-1)/6+1;
	}
	else no_of_imdcts[0]=no_of_imdcts[1]=(isbound[0]-1)/18+1;

   }

   return ms_flag;
}

static inline void stereo_s(int l,float a[2],int pos,int ms_flag,int is_pos,struct AUDIO_HEADER *header)
{
float ftmp,Mi,Si;

	if (l>=576) return; /* brrr... */

        if ((is_pos != IS_ILLEGAL) && (header->ID==1)) {
                ftmp=fras2(is[0][l],a[0]);
                xr[0][0][pos]=(1-t_is[is_pos])*ftmp;
                xr[1][0][pos]=t_is[is_pos]*ftmp;
		return;
	}

	if ((is_pos != IS_ILLEGAL) && (header->ID==0)) {
		ftmp=fras2(is[0][l],a[0]);
		if (is_pos&1) {
			xr[0][0][pos]= t_is2[intensity_scale][(is_pos+1)>>1] * ftmp;
			xr[1][0][pos]= ftmp;
		} else {
			xr[0][0][pos]= ftmp;
			xr[1][0][pos]= t_is2[intensity_scale][is_pos>>1] * ftmp;
		}
		return;
	}

        if (ms_flag) {
                Mi=fras2(is[0][l],a[0]);
                Si=fras2(is[1][l],a[1]);
                xr[0][0][pos]=(Mi+Si)*i_sq2;
                xr[1][0][pos]=(Mi-Si)*i_sq2;
        } else {
                xr[0][0][pos]=fras2(is[0][l],a[0]);
                xr[1][0][pos]=fras2(is[1][l],a[1]);
        }
}

static inline void stereo_l(int l,float a[2],int ms_flag,int is_pos,struct AUDIO_HEADER *header)
{
float ftmp,Mi,Si;
	if (l>=576) return;

/* new code by ???
*/
	if (is_pos != IS_ILLEGAL) {
		ftmp = fras2(is[0][l], a[0]);
		if (header -> ID ==1) {
			xr[0][0][l] = (1 - t_is[is_pos]) * ftmp;
			xr[1][0][l] = t_is[is_pos] * ftmp;
			return;
		} else if (is_pos & 1) {
			xr[0][0][l] = t_is2[intensity_scale][(is_pos + 1) >> 1] * ftmp;
			xr[1][0][l] = ftmp;
		} else {
			xr[0][0][l] = ftmp;
			xr[1][0][l] = t_is2[intensity_scale][is_pos >> 1] * ftmp;
		}
		return;
	}

	if (ms_flag) {
		Mi=fras2(is[0][l],a[0]);
		Si=fras2(is[1][l],a[1]);
		xr[0][0][l]=(Mi+Si)*i_sq2;
		xr[1][0][l]=(Mi-Si)*i_sq2;
	} else {
		xr[0][0][l]=fras2(is[0][l],a[0]);
		xr[1][0][l]=fras2(is[1][l],a[1]);
	}

}


/*
 * requantize_ms ***************************************************************
 */
void requantize_ms(int gr,struct SIDE_INFO *info,struct AUDIO_HEADER *header)
{
int l,sfb,ms_flag,is_pos,i,ch;
int *global_gain,subblock_gain[2],*scalefac_scale,scalefac[2],isbound[3];
int sfreq=header->sampling_frequency;
int id = header->ID;
float a[2];

global_gain=info->global_gain[gr];
scalefac_scale=info->scalefac_scale[gr];

        if (info->window_switching_flag[gr][0] && info->block_type[gr][0]==2)  
	{
                if (info->mixed_block_flag[gr][0]) 
                {
	/* 
	 * mixed blocks w/stereo processing - long block part ******************
	 */
                        int window,window_len;
                        int preflag[2]={0,0};

			ms_flag=find_isbound(isbound,gr,info,header);

			sfb=0; l=0;
			for (ch=0;ch<2;ch++) {
				scalefac[ch]=scalefac_l[gr][ch][0];
				a[ch]=fras_l(0,global_gain[ch],scalefac_scale[ch],scalefac[ch],preflag[ch]);
			}


			while (l<36) {
				int is_pos;
				if (l<isbound[0]) 
					is_pos=IS_ILLEGAL;
				else 
				{
					is_pos=scalefac[1];
					if (id==1) /* MPEG1 */
					{
						if (is_pos==7) 
							is_pos=IS_ILLEGAL;
					}
					else /* MPEG2 */
					{
						if (is_pos==is_max[sfb]) 
							is_pos=IS_ILLEGAL;
					}
				}

				stereo_l(l,a,ms_flag,is_pos,header);

				if (l==t_l[sfb]) 
				{
					sfb++;
					for (ch=0;ch<2;ch++) 
					{
						scalefac[ch]=scalefac_l[gr][ch][sfb];
						a[ch]=fras_l(sfb,global_gain[ch],scalefac_scale[ch],scalefac[ch],preflag[ch]);
					}
				}

				l++;
			}
	/*
	 * mixed blocks w/stereo processing - short block part *****************
	 */
                        sfb=3;
                        window_len=t_s[sfb]-t_s[sfb-1];

                        while (l<(MAX(non_zero[0],non_zero[1]))) 
                        {
                                for (window=0;window<3;window++) 
                                {
                                        subblock_gain[0]=info->subblock_gain[gr][0][window];
					subblock_gain[1]=info->subblock_gain[gr][1][window];
					scalefac[0]=scalefac_s[gr][0][sfb][window];
					scalefac[1]=scalefac_s[gr][1][sfb][window];

					if (t_s[sfb] < isbound[window]) 
					{
						is_pos=IS_ILLEGAL;
						a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
						a[1]=fras_s(global_gain[1],subblock_gain[1],scalefac_scale[1],scalefac[1]);
					} 
					else 
					{
						is_pos=scalefac[1];
						if (id==1) /* MPEG1 */
						{
							if (is_pos==7) 
								is_pos=IS_ILLEGAL;
						}
						else /* MPEG2 */
						{
							if (is_pos==is_max[sfb+6]) 
								is_pos=IS_ILLEGAL;
						} 
						a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
					}

                                        for (i=0;i<window_len;i++) 
                                        {
                                                stereo_s(l,a,t_reorder[id][sfreq][l],ms_flag,is_pos,header);
                                                l++;
                                        }
                                }
                                sfb++;
                                window_len=t_s[sfb]-t_s[sfb-1];
                        }
                        while (l<576) 
                        {
			        int reorder = t_reorder[id][sfreq][l++];
			  
				xr[0][0][reorder]=xr[1][0][reorder]=0;
			}
                } 
                else 
                {                                                                       
        /*
	 * requantize_ms - short blocks w/stereo processing ********************
	 */
                        int window,window_len;

			ms_flag=find_isbound(isbound,gr,info,header);	
			sfb=0; l=0;
			window_len=t_s[0]+1;

                        while (l<(MAX(non_zero[0],non_zero[1]))) 
                        {
                                for (window=0;window<3;window++) 
                                {
					subblock_gain[0]=info->subblock_gain[gr][0][window];
					subblock_gain[1]=info->subblock_gain[gr][1][window];
					scalefac[0]=scalefac_s[gr][0][sfb][window];
					scalefac[1]=scalefac_s[gr][1][sfb][window];

                                        if (t_s[sfb] < isbound[window]) 
                                        {
                                                is_pos=IS_ILLEGAL;
						a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
						a[1]=fras_s(global_gain[1],subblock_gain[1],scalefac_scale[1],scalefac[1]);
                                        } 
                                        else 
                                        {
                                                is_pos=scalefac[1];
                                                if (id==1) /* MPEG1 */
                                        	{
                                                        if (is_pos==7) 
                                                        	is_pos=IS_ILLEGAL;
						}
                                                else /* MPEG2 */
                                                {
                                                        if (is_pos==is_max[sfb+6]) 
                                                        	is_pos=IS_ILLEGAL;
						}
                                                a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
                                        }

                                        for (i=0;i<window_len;i++) 
                                        {
						stereo_s(l,a,t_reorder[id][sfreq][l],ms_flag,is_pos,header);
						l++;
					}
                                }
                                /* this won't do anymore
				 * window_len=-t_s[sfb]+t_s[++sfb];
				 */
				window_len  = -t_s[sfb++];
				window_len +=  t_s[sfb];

                        }
                        while (l<576) 
                        {
			        int reorder = t_reorder[id][sfreq][l++];
			  
				xr[0][0][reorder]=xr[1][0][reorder]=0;
			}
                }
        }
        else 
        {
        /*
	 * long blocks w/stereo processing *************************************
	 */
                int *preflag=info->preflag[gr];

		ms_flag=find_isbound(isbound,gr,info,header);

		sfb=0; l=0;
		scalefac[0]=scalefac_l[gr][0][sfb];
		a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
		scalefac[1]=scalefac_l[gr][1][sfb];
		a[1]=fras_l(sfb,global_gain[1],scalefac_scale[1],scalefac[1],preflag[1]);

		/* no intensity stereo part
		*/
	if (ms_flag)
                while (l< isbound[0]) {
#if defined(PENTIUM_RDTSC)

unsigned int cnt4, cnt3, cnt2, cnt1;
static int min_cycles = 99999999;

        __asm__(".byte 0x0f,0x31" : "=a" (cnt1), "=d" (cnt4));
#endif

			{
				register float Mi = fras2(is[0][l],a[0]);
				register float Si = fras2(is[1][l],a[1]);
				register float tmp = i_sq2;
				xr[0][0][l]=(Mi+Si)*tmp;
				xr[1][0][l]=(Mi-Si)*tmp;
			}

#if defined(PENTIUM_RDTSC)
                        __asm__(".byte 0x0f,0x31" : "=a" (cnt2), "=d" (cnt4));

                        if (cnt2-cnt1 < min_cycles) {
                          min_cycles = cnt2-cnt1;
                          /*printf("%d cycles\n", min_cycles);*/
                        }

#endif
                        if (l==t_l[sfb]) {
#if defined(PENTIUM_RDTSC2)

unsigned int cnt4, cnt2, cnt1;
static int min_cycles = 99999999;

        __asm__(".byte 0x0f,0x31" : "=a" (cnt1), "=d" (cnt4));
#endif

                                sfb++;
				scalefac[0]=scalefac_l[gr][0][sfb];
				a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
				scalefac[1]=scalefac_l[gr][1][sfb];
				a[1]=fras_l(sfb,global_gain[1],scalefac_scale[1],scalefac[1],preflag[1]);
#if defined(PENTIUM_RDTSC2)
                        __asm__(".byte 0x0f,0x31" : "=a" (cnt2), "=d" (cnt4));

                        if (cnt2-cnt1 < min_cycles) {
                          min_cycles = cnt2-cnt1;
                          /*printf("%d cycles, sfb %d\n", min_cycles, sfb);*/
                        }

#endif
                        }
                        l++;
                }
	else
                while (l< isbound[0]) {
                        xr[0][0][l]=fras2(is[0][l],a[0]);
                        xr[1][0][l]=fras2(is[1][l],a[1]);
                        if (l==t_l[sfb]) {
                                sfb++;
                                scalefac[0]=scalefac_l[gr][0][sfb];
                                a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
                                scalefac[1]=scalefac_l[gr][1][sfb];
                                a[1]=fras_l(sfb,global_gain[1],scalefac_scale[1],scalefac[1],preflag[1]);
                        }
                        l++;
                }


		/* intensity stereo part
		*/
		while (l<(MAX(non_zero[0],non_zero[1]))) {
			int is_pos=scalefac[1];
	
			if (id==1) /* MPEG1 */
			{
				if (is_pos==7) 
					is_pos=IS_ILLEGAL;
			}
			else /* MPEG2 */
			{
				if (is_pos==is_max[sfb]) 
					is_pos=IS_ILLEGAL;
			}
			stereo_l(l,a,ms_flag,is_pos,header);

			if (l==t_l[sfb]) 
			{
				sfb++;
				scalefac[0]=scalefac_l[gr][0][sfb];
				scalefac[1]=scalefac_l[gr][1][sfb];
				a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
			}
			l++;
		}

                while (l<576) {
			xr[0][0][l]=0;
			xr[1][0][l]=0;
			l++;
		}
        }
}

/*
 * requantize_downmix **********************************************************
 */
void requantize_downmix(int gr,struct SIDE_INFO *info,struct AUDIO_HEADER *header)
{
int l,sfb,ms_flag,i;
int *global_gain,subblock_gain[2],*scalefac_scale,scalefac[2];
int sfreq=header->sampling_frequency;
int id = header->ID;
float a[2];

	/* first set some variables
	*/
	global_gain=info->global_gain[gr];
	scalefac_scale=info->scalefac_scale[gr];

	if (header->mode_extension==2 || header->mode_extension==3) ms_flag=1;
	else ms_flag=0;

	/* ... and then we're off for requantization
	*/
	if (info->window_switching_flag[gr][0] && info->block_type[gr][0]==2)
	{
		if (info->mixed_block_flag[gr][0]) 
		{
			die("mixed block? hmmmm...., downmix for mixed blocks is not yet implemented\n");
		} 
		else 
		{
			int window,window_len;
			int isbound[3];
			int is_pos;
			
			find_isbound(isbound,gr,info,header); /* ugly hack, part3 */

			sfb=0; l=0; window_len=t_s[0]+1;
			
			while (l<(MAX(non_zero[0],non_zero[1]))) 
			{
				for (window=0;window<3;window++) 
				{
					subblock_gain[0]=info->subblock_gain[gr][0][window];
					subblock_gain[1]=info->subblock_gain[gr][1][window];
					scalefac[0]=scalefac_s[gr][0][sfb][window];
					is_pos=scalefac[1]=scalefac_s[gr][1][sfb][window];

					if (t_s[sfb] < isbound[window]) 
					{
						a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
						if (ms_flag) 
						{
							for (i=0;i<window_len;i++) 
							{
								register float Mi=fras2(is[0][l],a[0]);
								xr[0][0][t_reorder[id][sfreq][l]]=Mi*i_sq2;
								l++;
							}
						} 
						else 
						{
							a[1]=fras_s(global_gain[1],subblock_gain[1],scalefac_scale[1],scalefac[1]);
							for (i=0;i<window_len;i++) 
							{
								register float tmp1=fras2(is[0][l],a[0]);
								register float tmp2=fras2(is[1][l],a[1]);
								xr[0][0][t_reorder[id][sfreq][l]]=(tmp1+tmp2)*0.5f;
								l++;
							}
						}
					} 
					else 
					{
						a[0]=fras_s(global_gain[0],subblock_gain[0],scalefac_scale[0],scalefac[0]);
						for (i=0;i<window_len;i++) 
						{
							register float ftmp = fras2(is[0][l], a[0]);
							if (id==0 && is_pos<is_max[sfb])
								ftmp*=t_downmix[intensity_scale][(is_pos+1)>>1];
							xr[0][0][t_reorder[id][sfreq][l]] = ftmp;
							l++;
						}						
					}
				}
				window_len  = -t_s[sfb++];
				window_len +=  t_s[sfb];
			}
			while (l<576) 
			{
				xr[0][0][l]=0;
				l++;
			}
		}
	}
	else 
	{
		int *preflag=info->preflag[gr];
		int isbound;

		if (header->mode_extension==1 || header->mode_extension==3) 
		{
			int tmp=non_zero[1];
			while (is[1][tmp] == 0) tmp--;
			sfb=0; while (t_l[sfb] < tmp && sfb < 21) sfb++;
			isbound=t_l[sfb]+1;
			no_of_imdcts[0]=no_of_imdcts[1]=(non_zero[0]-1)/18+1;
		} 
		else 
		{
			isbound=(MAX(non_zero[0],non_zero[1]));
			no_of_imdcts[0]=no_of_imdcts[1]=(isbound-1)/18+1;
		}

                sfb=0; l=0;
                scalefac[0]=scalefac_l[gr][0][sfb];
                a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
                scalefac[1]=scalefac_l[gr][1][sfb];
                a[1]=fras_l(sfb,global_gain[1],scalefac_scale[1],scalefac[1],preflag[1]);		

		/* no intensity stereo part
		 */
		if (ms_flag)
		{
			while (l < isbound) 
			{
				register float Mi = fras2(is[0][l],a[0]);
				register float tmp = i_sq2;
				xr[0][0][l]=Mi*tmp;

				if (l==t_l[sfb]) 
				{
					sfb++;
                                	scalefac[0]=scalefac_l[gr][0][sfb];
                                	a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
					scalefac[1]=scalefac_l[gr][1][sfb];
				}
				l++;
			}
		}
		else 
		{
			while (l < isbound) 
			{
				register float tmp1=fras2(is[0][l],a[0]);
				register float tmp2=fras2(is[1][l],a[1]);
				xr[0][0][l]=(tmp1+tmp2)*0.5f;
	                        if (l==t_l[sfb]) 
	                        {
        	                        sfb++;
                	                scalefac[0]=scalefac_l[gr][0][sfb];
                        	        a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
                                	scalefac[1]=scalefac_l[gr][1][sfb];
                                	a[1]=fras_l(sfb,global_gain[1],scalefac_scale[1],scalefac[1],preflag[1]);
                        	}
                        	l++;
			}
		}
		/* intensity stereo part
		*/
		while (l<(MAX(non_zero[0],non_zero[1]))) 
		{
			int is_pos=scalefac[1];
			register float ftmp=fras2(is[0][l], a[0]);

			if (id==0  && is_pos<is_max[sfb]) 
			{
				ftmp*=t_downmix[intensity_scale][(is_pos+1)>>1];
			}

			xr[0][0][l] = ftmp;

			if (l==t_l[sfb]) 
			{
				sfb++;
				scalefac[0]=scalefac_l[gr][0][sfb];
				a[0]=fras_l(sfb,global_gain[0],scalefac_scale[0],scalefac[0],preflag[0]);
				scalefac[1]=scalefac_l[gr][1][sfb];
			}
			l++;
		}

		/* _always_ zero out everything else
		*/
		while (l<576) 
		{
			xr[0][0][l]=0;
			l++;
		}
	}
}

/* 
 * antialiasing butterflies ****************************************************
 * 
 */
void alias_reduction(int ch)
{
unsigned int sb;

        for (sb=1;sb<32;sb++) 
        {
	        float *x = xr[ch][sb];
		register float a, b;

	        a = x[0];
		b = x[-1];
                x[-1] = b * Cs[0] - a * Ca[0];
                x[0]  = a * Cs[0] + b * Ca[0];

	        a = x[1];
		b = x[-2];
                x[-2] = b * Cs[1] - a * Ca[1];
                x[1]  = a * Cs[1] + b * Ca[1];

	        a = x[2];
		b = x[-3];
                x[-3] = b * Cs[2] - a * Ca[2];
                x[2]  = a * Cs[2] + b * Ca[2];

	        a = x[3];
		b = x[-4];
                x[-4] = b * Cs[3] - a * Ca[3];
                x[3]  = a * Cs[3] + b * Ca[3];

	        a = x[4];
		b = x[-5];
                x[-5] = b * Cs[4] - a * Ca[4];
                x[4]  = a * Cs[4] + b * Ca[4];

	        a = x[5];
		b = x[-6];
                x[-6] = b * Cs[5] - a * Ca[5];
                x[5]  = a * Cs[5] + b * Ca[5];

	        a = x[6];
		b = x[-7];
                x[-7] = b * Cs[6] - a * Ca[6];
                x[6]  = a * Cs[6] + b * Ca[6];

	        a = x[7];
		b = x[-8];
                x[-8] = b * Cs[7] - a * Ca[7];
                x[7]  = a * Cs[7] + b * Ca[7];
	}
}

/* calculating t_43 instead of having that big table in misc2.h
 */

void calculate_t43(void)
{
int i;
	for (i=0;i<8192;i++)
		t_43[i]=(float)pow((float)i,1.33333333333f);
}