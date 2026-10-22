/***************************************************************************
 *  Copyright (C) 2026  Andreas Hanft                                      *
 *                                                                         *
 *  This program is free software: you can redistribute it and/or modify   *
 *  it under the terms of the GNU General Public License as published by   *
 *  the Free Software Foundation, either version 3 of the License, or      *
 *  (at your option) any later version.                                    *
 *                                                                         *
 *  This program is distributed in the hope that it will be useful,        *
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *  GNU General Public License for more details.                           *
 *                                                                         *
 *  You should have received a copy of the GNU General Public License      *
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.  *
 ***************************************************************************/


// util::Sha256, which checks the upscaler's download

#include "check.hpp"

#include "util/sha256.hpp"

#include <string>


namespace {

std::string sha256(const std::string &text)
{
    util::Sha256 sha;
    sha.add(text.data(), text.size());
    return sha.hex();
}

} // namespace


int main()
{
    // FIPS 180-2's examples, and inputs around the 56 bytes where the length takes a block of its own
    CHECK_EQUAL(sha256(""), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK_EQUAL(sha256("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK_EQUAL(sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"),
                "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    CHECK_EQUAL(sha256(std::string(1000000, 'a')), "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
    // fed in pieces, the same
    util::Sha256 pieces;
    const std::string text = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    for (char c : text)
        pieces.add(&c, 1);
    CHECK_EQUAL(pieces.hex(), "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    return test::result();
}
