#include <boost/assign/std/vector.hpp>
#include <cxxtest/TestSuite.h>
#include <libgeodecomp/misc/stdcontaineroverloads.h>

using namespace boost::assign;
using namespace LibGeoDecomp;

namespace LibGeoDecomp {

class StdContainerOverloadsTest_StdMap : public CxxTest::TestSuite
{
public:
    void testOperatorLessLess()
    {
        std::map<int, int> a;
        a[0] = 1;
        a[1] = 2;
        a[2] = 3;
        std::ostringstream temp;
        temp << a;
        TS_ASSERT_EQUALS("{0 => 1, 1 => 2, 2 => 3}", temp.str());
    }
};

class StdContainerOverloadsTest_StdVector : public CxxTest::TestSuite
{
public:

    void deleteChecker(int excludeObj)
    {
        // create test object
        int size = 7;
        std::vector<int> original(size);
        for (int i = 0; i < size - 2; i++) {
            original[i] = 1 << i;
        }

        original[size - 2] = 4;
        original[size - 1] = 4;
        del(original, excludeObj);

        // create reference
        std::vector<int> cropped;
        for (int i = 0; i < size - 2; i++) {
            int val = 1 << i;
            if (val != excludeObj) {
                cropped.push_back(val);
            }
        }

        if (4 != excludeObj) {
            cropped.push_back(4);
            cropped.push_back(4);
        }

        TS_ASSERT_EQUALS(original, cropped);
    }

    void testConstructor()
    {
        std::vector<int> expected;
        expected.push_back(4);
        expected.push_back(7);
        expected.push_back(11);
        std::vector<int> actual(expected.begin(), expected.end());
        TS_ASSERT_EQUALS(actual.size(), expected.size());
        TS_ASSERT_EQUALS(actual[0], expected[0]);
        TS_ASSERT_EQUALS(actual[1], expected[1]);
        TS_ASSERT_EQUALS(actual[2], expected[2]);
    }

    void testDelete()
    {
        deleteChecker(-1);
        deleteChecker(1);
        deleteChecker(4);
        deleteChecker(16);
    }

    void testPop()
    {
        std::vector<int> stack;
        stack << 1
              << 2
              << 3;
        TS_ASSERT_EQUALS(3, pop(stack));
        TS_ASSERT_EQUALS(2, pop(stack));
        TS_ASSERT_EQUALS(1, pop(stack));
    }

    void testPopFront()
    {
        std::vector<int> stack;
        stack << 1
              << 2
              << 3;
        TS_ASSERT_EQUALS(1, pop_front(stack));
        TS_ASSERT_EQUALS(2, pop_front(stack));
        TS_ASSERT_EQUALS(3, pop_front(stack));
    }

    void testPushFront()
    {
        std::vector<int> a;
        a += 47, 11, 2000;

        std::vector<int> b;
        b += 11, 2000;
        push_front(b, 47);

        TS_ASSERT_EQUALS(a, b);
    }

    void testSum()
    {
        std::vector<int> s;
        s += 12, 43, -9, -8, 15;
        TS_ASSERT_EQUALS(53, sum(s));
    }

    void testAppend()
    {
        std::vector<int> a;
        a += 1, 2, 3;
        std::vector<int> b;
        b += 4, 5;
        std::vector<int> c;
        c += 1, 2, 3, 4, 5;

        TS_ASSERT_EQUALS(a + b, c);
        append(a, b);
        TS_ASSERT_EQUALS(a, c);
    }

    void testOperatorLessLess()
    {
        std::vector<int> a;
        a += 1, 2, 3;
        std::ostringstream temp;
        temp << a;
        TS_ASSERT_EQUALS("[1, 2, 3]", temp.str());
    }

    void testContains()
    {
        std::vector<int> a;
        a += 0, 1;
        TS_ASSERT_EQUALS(contains(a, 2), false);
        TS_ASSERT_EQUALS(contains(a, 1), true);
    }

    void testSort()
    {
        std::vector<unsigned> v;
        std::vector<unsigned> w;
        v += 0, 3, 1, 2;
        w += 0, 1, 2, 3;
        sort(v);
        TS_ASSERT_EQUALS(v, w);
    }

    void testMax()
    {
        std::vector<unsigned> a;
        a += 0, 3, 1 ,2;
        TS_ASSERT_EQUALS((max)(a), (unsigned)3);
    }
};

};