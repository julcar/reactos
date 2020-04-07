// Copyright (C) 2003 by Oren Avissar
// (go to www.dftpd.com for more information)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
//
// BlowfishCrypt.h
//
//    Encryption and decryption of byte strings.
//
//    CAVEATS: The last (n % 8) bytes of *buf, if any, will be untouched, so 
//    pad accordingly.  Operates in cipher block chaining mode, so the Block 
//    argument and the buffer size must be the same for Encrypt and Decrypt.
//
//    Reorganized by Oren Avissar 5/12/02
//
////////////////////////////////////////////////////////////////////////////
#ifndef BLOWFISHCRYPT_H
#define BLOWFISHCRYPT_H

class CBlowfishCrypt {
public:
   struct Block { 
      unsigned long l, r;
      Block& operator^=(Block& b) { l ^= b.l; r ^= b.r; return *this; } 
   };

    CBlowfishCrypt();
    virtual ~CBlowfishCrypt();

        //Initialize the P and S boxes
    void Initialize(const unsigned char* szKey);

        //Encrypt/decrypt buffer in place
    void Encrypt(unsigned char* p, size_t &n, Block& chain);
    void Decrypt(unsigned char* p, size_t n, Block& chain);

       //Encrypt/decrypt from input buffer to output
    void Encrypt(unsigned char* q, const unsigned char* in, size_t &n, Block& chain);
    void Decrypt(unsigned char* q, const unsigned char* p,size_t n, Block& chain);

        //Get output length, which must be even MOD 8
    long GetOutputLength(long lInputLong);

private:
    unsigned long F(unsigned long);
    void X(Block&, const int);
    void Encrypt(Block&);
    void Decrypt(Block&);
    unsigned long P[18];
    unsigned long S[4][256];

    static const unsigned long Init_P[18];
    static const unsigned long Init_S[4][256];

    inline void BytesToBlock(CBlowfishCrypt::Block& b, const unsigned char* p);
    inline void BlockToBytes(unsigned char* p, CBlowfishCrypt::Block b);
};

#endif //BLOWFISHCRYPT_H
