#include "c_assert.hpp"
#include <iostream>
#include <random>
#include <tfhe++.hpp>

int main()
{
    constexpr uint32_t num_test = 1000;
    std::random_device seed_gen;
    std::default_random_engine engine(seed_gen());
    std::uniform_int_distribution<uint32_t> binary(0, 1);
    bool pass_flag = true;
    std::cout << "test p=1" << std::endl;

    std::cout << "lvl1" << std::endl;
    for (int test = 0; test < num_test; test++) {
        TFHEpp::lweKey key;

        std::array<bool, TFHEpp::lvl1param::n> p;
        for (bool &i : p) i = (binary(engine) > 0);
        TFHEpp::Polynomial<TFHEpp::lvl1param> pmu;
        for (int i = 0; i < TFHEpp::lvl1param::n; i++)
            pmu[i] = p[i] ? TFHEpp::lvl1param::mu : -TFHEpp::lvl1param::mu;
        TFHEpp::TRLWE<TFHEpp::lvl1param> c =
            TFHEpp::trlweSymEncrypt<TFHEpp::lvl1param>(pmu, key.lvl1);

        const TFHEpp::Polynomial<TFHEpp::lvl1param> plainpoly = {
            static_cast<typename TFHEpp::lvl1param::T>(1)};

        TFHEpp::TRGSWRAINTT<TFHEpp::lvl1param> trgswntt =
            TFHEpp::trgswrainttSymEncrypt<TFHEpp::lvl1param>(plainpoly,
                                                             key.lvl1);
        TFHEpp::trgswrainttExternalProduct<TFHEpp::lvl1param>(c, c, trgswntt);
        // if constexpr(TFHEpp::hasq<TFHEpp::lvl1param>) for(int
        // i=0;i<TFHEpp::lvl1param::n;i++) assert(c[0][i]<TFHEpp::lvl1param::q);
        std::array<bool, TFHEpp::lvl1param::n> p2 =
            TFHEpp::trlweSymDecrypt<TFHEpp::lvl1param>(c, key.lvl1);
        for (int i = 0; i < TFHEpp::lvl1param::n; i++)
            if (p[i] != p2[i])
                std::cout << i << ":" << (p[i] ? 1 : 0) << ":"
                          << (p2[i] ? 1 : 0) << std::endl;
        for (int i = 0; i < TFHEpp::lvl1param::n; i++) {
            if (p[i] != p2[i]) {
                std::cout << "Test " << i << " Failed" << std::endl;
                pass_flag = false;
                break;
            }
            c_assert(p[i] == p2[i]);
        }
        if(!pass_flag) break;
    }
    if(pass_flag) std::cout << "Passed" << std::endl;
    pass_flag = true;
    std::cout << "test p=-1" << std::endl;

    std::cout << "lvl1" << std::endl;
    for (int test = 0; test < num_test; test++) {
        TFHEpp::lweKey key;

        std::array<bool, TFHEpp::lvl1param::n> p;
        for (bool &i : p) i = binary(engine) > 0;
        std::array<typename TFHEpp::lvl1param::T, TFHEpp::lvl1param::n> pmu;
        for (int i = 0; i < TFHEpp::lvl1param::n; i++)
            pmu[i] = p[i] ? TFHEpp::lvl1param::mu : -TFHEpp::lvl1param::mu;
        TFHEpp::TRLWE<TFHEpp::lvl1param> c =
            TFHEpp::trlweSymEncrypt<TFHEpp::lvl1param>(pmu, key.lvl1);

        const TFHEpp::Polynomial<TFHEpp::lvl1param> plainpoly = {
            static_cast<typename TFHEpp::lvl1param::T>(-1)};

        TFHEpp::TRGSWRAINTT<TFHEpp::lvl1param> trgswntt =
            TFHEpp::trgswrainttSymEncrypt<TFHEpp::lvl1param>(plainpoly,
                                                             key.lvl1);
        TFHEpp::trgswrainttExternalProduct<TFHEpp::lvl1param>(c, c, trgswntt);
        std::array<bool, TFHEpp::lvl1param::n> p2 =
            TFHEpp::trlweSymDecrypt<TFHEpp::lvl1param>(c, key.lvl1);
        for (int i = 0; i < TFHEpp::lvl1param::n; i++) {
            if (p[i] == p2[i]) {
                std::cout << "Test " << i << " Failed" << std::endl;
                pass_flag = false;
                break;
            }
            c_assert(p[i] == !p2[i]);
        }
        if(!pass_flag) break;
    }
    if(pass_flag) std::cout << "Passed" << std::endl;
}