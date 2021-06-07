/*
* open-bo-api - C++ API for working with binary options brokers
*
* Copyright (c) 2020 Elektro Yar. Email: git.electroyar@gmail.com
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef OPEN_BO_API_CRC64_HPP_INCLUDED
#define OPEN_BO_API_CRC64_HPP_INCLUDED

namespace open_bo_api {
	
	class CRC64 {
	public:
		inline static const long long poly = 0xC96C5795D7870F42;
		inline static long long crc64_table[256];

		inline static void generate_table(){
			for(int i=0; i<256; ++i) {
				long long crc = i;
				for(int j=0; j<8; ++j) {
					if(crc & 1) {
						crc >>= 1;
						crc ^= poly;
					} else {
						crc >>= 1;
					}
				}
				crc64_table[i] = crc;
			}
		}

		inline static long long calc_crc64(long long crc, const unsigned char* stream, int n) {
			for(int i=0; i< n; ++i) {
				unsigned char index = stream[i] ^ crc;
				long long lookup = crc64_table[index];
				crc >>= 8;
				crc ^= lookup;
			}
			return crc;
		}

		inline static long long calc_crc64(const std::string str) {
			return calc_crc64(0, (const unsigned char*)str.c_str(), str.size());
		}
	};
}
#endif // CRC64_HPP_INCLUDED
