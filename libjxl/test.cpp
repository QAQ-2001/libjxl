// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

// This example encodes a file containing a floating point image to another
// file containing JPEG XL image with a single frame.
#pragma warning(disable:4996)
#include <limits.h>

#include <sstream>
#include <string.h>
#include <vector>

#include "jxl/encode.h"
#include "jxl/encode_cxx.h"
#include "jxl/thread_parallel_runner.h"
#include "jxl/thread_parallel_runner_cxx.h"

#include <iostream>
#include <windows.h>
#include <stdio.h>
#include <time.h>
#include <io.h>

JxlEncoderPtr g_enc = JxlEncoderMake(nullptr);
//不要对齐，有些bmp图像因为4字节的自动补位会出错? JxlDecoderImageOutBufferSize(dec.get(), &format, &buffer_size)的buffer_size!=x*y*3
//因为BMP会自动补齐！！！
//试试直接传原始数据进去，应该是会自动处理成4位补齐！
JxlPixelFormat g_pixel_format = { 3, JXL_TYPE_UINT8, JXL_LITTLE_ENDIAN, 0 };
JxlEncoderOptions* g_option = nullptr;

clock_t start, end;

//3-channel BMP
//BPM因为是Windows的小端endian，所以颜色BGR需要倒一下，
//同时BPM像素点是从左下到右上，所以像素点也需要倒一下。
bool ReadBMP(const char* filename, std::vector<uint8_t>* pixels, uint32_t* xsize, uint32_t* ysize) {
    BITMAPFILEHEADER bf;                      //图像文件头
    BITMAPINFOHEADER bi;                      //图像文件头信息

    FILE* file = fopen(filename, "rb");
    if (!file) 
    {
        fprintf(stderr, "Could not open %s for reading.\n", filename);
        return false;
    }
    fread(&bf, sizeof(BITMAPFILEHEADER), 1, file);//读取BMP文件头文件
    fread(&bi, sizeof(BITMAPINFOHEADER), 1, file);//读取BMP文件头文件信息

    //BOOL inverted = bi.biHeight < 0 ? TRUE : FALSE;//位图数据是否是倒立的

    *xsize = static_cast<uint32_t>(bi.biWidth);                            //获取图像的宽
    *ysize = static_cast<uint32_t>(bi.biHeight);                           //获取图像的高

    fseek(file, bf.bfOffBits, SEEK_SET);

    size_t size = (*xsize * *ysize * 3);
    pixels->resize(size);

    std::vector<char> data;
    data.resize(bf.bfSize - bf.bfOffBits);
    size_t readsize = fread(data.data(), size, 1, file);

    BOOL padding = (bf.bfSize - bf.bfOffBits) == size ? TRUE : FALSE;
    int skip;

    if (FALSE == padding)
    {
        skip = 4 - ((bi.biWidth * bi.biBitCount) >> 3) & 3;
    }
    else
    {
        skip = 0;
    }

    //std::cout << "skip: " << skip << std::endl;
   
    //颜色正确了，上下倒位
    //for (int y = 0; y < (int)*ysize; y++) {
    //    for (int x = 0; x < (int)*xsize; x++) {
    //        for (int c = 2; c >= 0; c--) {
    //            memcpy(pixels->data() + (y * *xsize + x) * 3 + c, data.data() + (y * *xsize + x) * 3 + (2 - c), sizeof(uint8_t));
    //        }
    //    }
    //}

    //正确了
    /*for (int y = 0; y < (int)*ysize; y++) 
    {
        for (int x = 0; x < (int)*xsize; x++) 
        {
            for (int c = 2; c >= 0; c--) 
            {
                memcpy(pixels->data() + (y * *xsize + x) * 3 + c, 
                    data.data() + ((*ysize - y - 1) * *xsize + x) * 3 + (2 - c),
                    sizeof(uint8_t));
            }
        }
    }*/
    //if (FALSE == inverted)
    //{
    //    for (int y = 0; y < (int)*ysize; y++)
    //    {
    //        for (int x = 0; x < (int)*xsize; x++)
    //        {
    //            for (int c = 2; c >= 0; c--)
    //            {
    //                memcpy(pixels->data() + (y * *xsize + x) * 3 + c,
    //                    data.data() + (y * *xsize + x) * 3 + (2 - c) + skip * y,
    //                    //2-c换BGR->RGB，skip * y补齐
    //                    sizeof(uint8_t));
    //            }
    //        }
    //    }

    //    if (fclose(file) != 0)
    //    {
    //        return false;
    //    }

    //    return true;
    //}
    for (int y = 0; y < (int)*ysize; y++)
    {
        for (int x = 0; x < (int)*xsize; x++)
        {
            for (int c = 2; c >= 0; c--)
            {
                memcpy(pixels->data() + (y * *xsize + x) * 3 + c,
                    data.data() + ((*ysize - y - 1) * *xsize + x) * 3 + (2 - c) + skip * (*ysize - y - 1),
                    //2-c换BGR->RGB，(*ysize - y - 1)换图像位置，skip * (*ysize - y - 1)补齐
                    sizeof(uint8_t));
            }
        }
    }

    if (fclose(file) != 0) 
    {
        return false;
    }

    //std::stringstream datastream;
    //std::string datastream_content(data.data(), data.size());
    //datastream.str(datastream_content);

    /*for (int y = 0; y < (int)*ysize; y++) {
        for (int x = 0; x < (int)*xsize; x++) {
            for (int c = 0; c < 3; c++) {
                memcpy(pixels->data() + (y * *xsize + x) * 3 + c, data.data() + (y * *xsize + x) * 3 + c, sizeof(uint8_t));
            }
        }
    }*/

    return true;
}

bool ReadPPM(const char* filename, std::vector<uint8_t>* pixels, uint32_t* xsize, uint32_t* ysize) 
{
    FILE* file = fopen(filename, "rb");
    if (!file) 
    {
        fprintf(stderr, "Could not open %s for reading.\n", filename);
        return false;
    }

    int width, height;
    char header[20];
    memset(header, 0, 20);
    fgets(header, 20, file);// get "P6" 
    fgets(header, 20, file);// get "width height" 
    sscanf_s(header, "%d %d\n", &width, &height);
    fgets(header, 20, file);// get "255"

    *xsize = width;
    *ysize = height;

    size_t size = (*xsize * *ysize * 3);
    std::vector<char> data;
    data.resize(size);
    pixels->resize(size);

    size_t readsize = fread(data.data(), 1, size, file);
    
    for (int y = 0; y < (int)*ysize; y++) 
    {
        for (int x = 0; x < (int)*xsize; x++) 
        {
            for (int c = 2; c >= 0; c--) 
            {
                memcpy(pixels->data() + (y * *xsize + x) * 3 + c, data.data() + (y * *xsize + x) * 3 + c, sizeof(uint8_t));
            }
        }
    }

    if ((long)readsize != size)
    {
        return false;
    }
    if (fclose(file) != 0)
    {
        return false;
    }

    return true;
}

bool JxlEncodeInit(const uint32_t x, const uint32_t y)
{
    JxlBasicInfo basic_info;
    JxlEncoderInitBasicInfo(&basic_info);
    basic_info.xsize = x;
    basic_info.ysize = y;
    basic_info.bits_per_sample = 8;
    basic_info.exponent_bits_per_sample = 0;
    basic_info.uses_original_profile = JXL_FALSE;

    if (JXL_ENC_SUCCESS != JxlEncoderSetBasicInfo(g_enc.get(), &basic_info))
    {
        printf("JxlEncoderSetBasicInfo failed\n");
        return false;
    }

    JxlColorEncoding color_encoding = {};
    JxlColorEncodingSetToSRGB(&color_encoding, false);
    if (JXL_ENC_SUCCESS != JxlEncoderSetColorEncoding(g_enc.get(), &color_encoding))
    {
        printf("JxlEncoderSetColorEncoding failed\n");
        return false;
    }

    g_option = JxlEncoderOptionsCreate(g_enc.get(), nullptr);

    //JxlEncoderOptionsSetEffort(g_option, 1);

    //decodeing speed 4-fastest 0-slowest
    if (JXL_ENC_SUCCESS != JxlEncoderOptionsSetDecodingSpeed(g_option, 4))
    {
        fprintf(stderr, "JxlEncoderOptionsSetDecodingSpeed failed\n");
        return false;
    }

    if (JXL_ENC_SUCCESS != JxlEncoderOptionsSetEffort(g_option, 1))
    {
        fprintf(stderr, "JxlEncoderOptionsSetEffort failed\n");
        return false;
    }

    return true;
}

/**
 * Compresses the provided pixels.
 *
 * @param pixels input pixels
 * @param xsize width of the input image
 * @param ysize height of the input image
 * @param compressed will be populated with the compressed bytes
 */
bool EncodeJxlOneshot(const std::vector<uint8_t>& pixels, std::vector<uint8_t>* compressed, const uint32_t x, const uint32_t y)
{
    start = clock();
    //线程池函数，请放在操作里
    auto runner = JxlThreadParallelRunnerMake(nullptr, JxlThreadParallelRunnerDefaultNumWorkerThreads());
    if (JXL_ENC_SUCCESS != JxlEncoderSetParallelRunner(g_enc.get(), JxlThreadParallelRunner, runner.get()))
    {
        fprintf(stderr, "JxlEncoderSetParallelRunner failed\n");
        return false;
    }

    if (JXL_ENC_SUCCESS != JxlEncoderAddImageFrame(g_option, &g_pixel_format, (void*)pixels.data(), sizeof(uint8_t) * pixels.size()))
    {
        fprintf(stderr, "JxlEncoderAddImageFrame failed\n");
        return false;
    }

    //compressed->resize(2048);
    uint8_t* next_out = compressed->data();
    size_t avail_out = compressed->size();//- (next_out - compressed->data());
    //JxlEncoderStatus process_result = JXL_ENC_NEED_MORE_OUTPUT;

    /*while (process_result == JXL_ENC_NEED_MORE_OUTPUT) 
    {
        process_result = JxlEncoderProcessOutput(g_enc.get(), &next_out, &avail_out);
        if (process_result == JXL_ENC_NEED_MORE_OUTPUT) 
        {
            size_t offset = next_out - compressed->data();
            compressed->resize(compressed->size() * 2);
            next_out = compressed->data() + offset;
            avail_out = compressed->size() - offset;
        }
    }*/

    if (JXL_ENC_SUCCESS != JxlEncoderProcessOutput(g_enc.get(), &next_out, &avail_out))
    {
        fprintf(stderr, "JxlEncoderProcessOutput failed\n");
        return false;
    }

    end = clock();

    compressed->resize(next_out - compressed->data());

    std::cout << "time: " << end - start << "ms" << " compression rate: " 
        << (float)(compressed->size())/(float)(pixels.size()) << std::endl;

    std::cout << "compression speed: " << (float)(pixels.size()) / ((float)(end - start) * 1000) << "MB/s" << std::endl;

    return true;
}

/**
 * Writes bytes to file.
 */
bool WriteFile(const std::vector<uint8_t>& bytes, const char* filename) 
{
    std::cout << "output jxl: " << filename << std::endl;
    FILE* file = fopen(filename, "wb");
    if (!file) 
    {
        fprintf(stderr, "Could not open %s for writing\n", filename);
        return false;
    }
    if (fwrite(bytes.data(), sizeof(uint8_t), bytes.size(), file) != bytes.size()) 
    {
        fprintf(stderr, "Could not write bytes to %s\n", filename);
        return false;
    }
    if (fclose(file) != 0) 
    {
        fprintf(stderr, "Could not close %s\n", filename);
        return false;
    }
    return true;
}

int main(int argc, char* argv[]) 
{
    char inImagePath[40] = "C:\\image\\bmp\\artificial\\*.bmp";
    char inFile[40] = "C:\\image\\bmp\\artificial\\";
    char outImagePath[40] = "C:\\image\\jxl\\artificial\\";

    intptr_t handle;
    struct _finddata_t fileInfo;

    int i = 1;
    char num[10] = { 0 };
    itoa(i, num, 10);

    std::vector<uint8_t> pixels;
    uint32_t xsize;
    uint32_t ysize;

    handle = _findfirst(inImagePath, &fileInfo);
    char outFileName[40];
    strcpy(outFileName, outImagePath);
    strcat(outFileName, num);
    strcat(outFileName, ".jxl");
    char inFilename[40] = { 0 };
    strcpy(inFilename, inFile);
    strcat(inFilename, fileInfo.name);
    std::cout << fileInfo.name << " bmpSize:" << fileInfo.size << std::endl;

    if (ReadBMP(inFilename, &pixels, &xsize, &ysize))
    {
        //
        //if (!ReadPPM(pfm_filename, &pixels, &xsize, &ysize)) {
        //    fprintf(stderr, "Couldn't load %s\n", pfm_filename);
        //    return 2;
        //}

        if (!JxlEncodeInit(xsize, ysize))
        {
            fprintf(stderr, "Couldn't encode jxl\n");
            return 0;
        }

        size_t size = xsize * ysize * 3;
        std::vector<uint8_t> compressed;
        compressed.resize(size);
        if (!EncodeJxlOneshot(pixels, &compressed, xsize, ysize))
        {
            fprintf(stderr, "Couldn't encode jxl\n");
            return 3;
        }

        if (!WriteFile(compressed, outFileName))
        {
            fprintf(stderr, "Couldn't write jxl file\n");
            return 4;
        }

        JxlEncoderReset(g_enc.get());
    }
    else
    {
        std::cout << "error" << std::endl;
        return 0;
    }

    xsize = ysize = 0;
    pixels.clear();

    if (-1 == handle)
    {
        printf("[error]");
        return 0;
    }
    while (0 == _findnext(handle, &fileInfo))
    {
        std::cout << fileInfo.name << " bmpSize:" << fileInfo.size << std::endl;
        memset(outFileName, 0, 20);
        memset(inFilename, 0, 20);
        char numTemp[10] = { 0 };
        i++;
        itoa(i, numTemp, 10);
        strcpy(outFileName, outImagePath);
        strcat(outFileName, numTemp);
        strcat(outFileName, ".jxl");
        strcpy(inFilename, inFile);
        strcat(inFilename, fileInfo.name);
        if (ReadBMP(inFilename, &pixels, &xsize, &ysize))
        {
            //
            //if (!ReadPPM(pfm_filename, &pixels, &xsize, &ysize)) {
            //    fprintf(stderr, "Couldn't load %s\n", pfm_filename);
            //    return 2;
            //}

            if (!JxlEncodeInit(xsize, ysize))
            {
                fprintf(stderr, "Couldn't encode jxl\n");
                return 0;
            }

            size_t size = xsize * ysize * 3;
            std::vector<uint8_t> compressed;
            compressed.resize(size);
            if (!EncodeJxlOneshot(pixels, &compressed, xsize, ysize))
            {
                fprintf(stderr, "Couldn't encode jxl\n");
                return 3;
            }

            if (!WriteFile(compressed, outFileName))
            {
                fprintf(stderr, "Couldn't write jxl file\n");
                return 4;
            }

            JxlEncoderReset(g_enc.get());
        }
        else
        {
            std::cout << "error" << std::endl;
            return 0;
        }
        xsize = ysize = 0;
        pixels.clear();
    }

    _findclose(handle);

    return 0;
}