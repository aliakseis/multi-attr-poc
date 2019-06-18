// multi-attr-poc.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "kdtree.h"

#include "ofxHammingCode.h"
//#include "rs"

#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <array>

#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/gzip.hpp>

void MakeDummies(const std::string& list, AttributeType* pos, unsigned int coeff = 1)
{
    if (list == "\\N")
        return;

    std::vector<uint32_t> encoded;

    std::istringstream ss(list);
    std::string buffer;

    while (std::getline(ss, buffer, ','))
    {
        unsigned int id = 0;
        sscanf_s(buffer.c_str(), "nm%u", &id);
        id *= coeff;
        id &= 0x03FFFFFF;
        encoded.push_back(ofxHammingCode::H3126::SECDED::encode(id));
    }

    if (encoded.empty())
        return;

    for (int idx = 0; idx < 32; ++idx)
    {
        uint32_t mask = 1u << idx;
        int cnt = 0;
        for (const auto& v : encoded)
        {
            if (v & mask)
                ++cnt;
        }
        pos[idx] = AttributeType((cnt * 255) / encoded.size());
    }
}

/*
void MakeDummies(const std::string& list, AttributeType* pos)
{
    if (list == "\\N")
        return;

    std::vector<std::array<uint8_t, 4>> encoded;

    std::istringstream ss(list);
    std::string buffer;

    while (std::getline(ss, buffer, ','))
    {
        int id = 0;
        sscanf_s(buffer.c_str(), "nm%d", &id);
        //encoded.push_back(ofxHammingCode::H3126::SECDED::encode(id));
        ezpwd::RS<7, 6> rs; // Up to 7 symbols data load; adds symbols parity
        std::array<uint8_t, 4> data{ uint8_t(id), uint8_t(id >> 8), uint8_t(id >> 16), 0 };
        rs.encode(data); // Place the R-S parity symbol(s) at end of data
        encoded.push_back(data);
    }

    if (encoded.empty())
        return;

    for (int idx = 0; idx < 32; ++idx)
    {
        uint32_t mask = 1u << (idx % 8);
        int cnt = 0;
        for (const auto& v : encoded)
        {
            if (v[idx / 8] & mask)
                ++cnt;
        }
        pos[idx] = AttributeType((cnt * 255) / encoded.size());
    }
}
*/

ObjectInfo MakeObjectInfo(
    const std::string& tconst,
    const std::string& directors,
    const std::string& writers)
{
    int titleId = 0;
    sscanf_s(tconst.c_str(), "tt%d", &titleId);

    ObjectInfo result{};
    result.data = titleId;

    MakeDummies(directors, result.pos);
    MakeDummies(directors, result.pos + 32, 37);
    MakeDummies(writers, result.pos + 64);
    MakeDummies(writers, result.pos + 96, 37);

    return result;
}

struct Comparator
{
    bool operator()(const ObjectInfo& left, const ObjectInfo& right) const {
        return left.data < right.data;
    };
    bool operator()(int left, const ObjectInfo& right) const {
        return left < right.data;
    };
    bool operator()(const ObjectInfo& left, int right) const {
        return left.data < right;
    };
};

int main(int argc, char** argv) 
{
    try
    {
        std::ifstream file("title.crew.tsv.gz", std::ios_base::in | std::ios_base::binary);
        boost::iostreams::filtering_streambuf<boost::iostreams::input> inbuf;
        inbuf.push(boost::iostreams::gzip_decompressor());
        inbuf.push(file);
        //Convert streambuf to istream
        std::istream instream(&inbuf);

        if (!instream)
        {
            std::cerr << "Missing data file!\n";
            return 1;
        }

        ObjectInfos infos;

        infos.reserve(8000000);

        //Iterate lines
        bool isFirst = true;
        std::string line;
        while (std::getline(instream, line)) 
        {
            if (isFirst)
            {
                isFirst = false;
                continue;
            }

            std::istringstream ss(line);
            std::string tconst;
            // tconst
            std::getline(ss, tconst, '\t');
            // directors
            std::string directors;
            std::getline(ss, directors, '\t');
            // writers
            std::string writers;
            ss >> writers;

            infos.push_back(MakeObjectInfo(tconst, directors, writers));
        }
        //Cleanup
        file.close();

        std::sort(infos.begin(), infos.end(), Comparator());

        std::vector<ObjectInfo*> infoPtrs;
        infoPtrs.reserve(infos.size());

        for (auto& v : infos)
        {
            infoPtrs.push_back(&v);
        }

        auto root = insert(infoPtrs.begin(), infoPtrs.end(), nullptr, 0);


        int key = 0;
        while (std::cout << ">> ", std::cin >> key)
        {
            auto it = std::lower_bound(infos.begin(), infos.end(), key, Comparator());

            if (it == infos.end() || it->data != key)
                continue;
            
            SearchResults result(50);
            bool flags[DIM * 2]{};
            kd_nearest_i_nearer_subtree(root, it->pos, result, flags, 0);

            std::vector<SearchResult> results(result.data(), result.data() + result.size());
            std::sort(results.begin(), results.end());
            for (const auto& v : results)
            {
                std::cout << v.data << "; " << v.dist_sq << '\n';
            }
        }
    }
    catch (const std::exception& ex)
    {
        std::cerr << ex.what() << '\n';
        return 1;
    }
    return 0;
}
