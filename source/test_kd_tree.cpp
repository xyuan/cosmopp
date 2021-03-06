#include <utility>
#include <algorithm>

#include <macros.hpp>
#include <random.hpp>
#include <timer.hpp>
#include <numerics.hpp>
#include <kd_tree.hpp>
#include <test_kd_tree.hpp>

std::string
TestKDTree::name() const
{
    return std::string("KD TREE TESTER");
}

unsigned int
TestKDTree::numberOfSubtests() const
{
    return 9;
}

void
TestKDTree::runSubTest(unsigned int i, double& res, double& expected, std::string& subTestName)
{
    check(i >= 0 && i < numberOfSubtests(), "invalid index " << i);

    int dim, k, seed;
    unsigned long nPoints;

    bool testRes;

    switch(i)
    {
    case 0:
        runSubTest0(res, expected, subTestName);
        return;
    case 1:
        runSubTest1(res, expected, subTestName);
        return;
    case 2:
        runSubTest2(res, expected, subTestName);
        return;
    case 3:
        runSubTest3(res, expected, subTestName);
        return;
    case 4:
        runSubTest4(res, expected, subTestName);
        return;
    case 5:
        dim = 1;
        k = 2;
        nPoints = 1000;
        seed = 0;
        subTestName = "1_2_1000";
        break;
    case 6:
        dim = 3;
        k = 2;
        nPoints = 10000;
        seed = 0;
        subTestName = "3_2_10000";
        break;
    case 7:
        dim = 5;
        k = 10;
        nPoints = 1000000;
        seed = 0;
        subTestName = "5_10_1000000";
        break;
    case 8:
        dim = 10;
        k = 20;
        nPoints = 1000000;
        seed = 0;
        subTestName = "10_20_1000000";
        break;
    default:
        check(false, "");
        break;
    }

    testRes = test(dim, nPoints, k, seed);
    expected = 1;
    res = (testRes ? 1 : 0);
}

void
TestKDTree::runSubTest0(double& res, double& expected, std::string& subTestName)
{
    Math::UniformRealGenerator gen(std::time(0), -1, 1);

    int size = 1000;
    int dim = 5;

    std::vector<std::vector<double> > points(size);
    for(int i = 0; i < size; ++i)
    {
        points[i].resize(dim);
        for(int j = 0; j < dim; ++j)
            points[i][j] = gen.generate();
    }

    KDTree kdTree(dim, points);

    res = 10;
    expected = kdTree.depth();
    subTestName = "depth";
}

void
TestKDTree::runSubTest1(double& res, double& expected, std::string& subTestName)
{
    std::vector<std::vector<double> > points(3);
    for(int i = 0; i < 3; ++i)
        points[i].resize(1, 0);

    points[1][0] = -1;
    points[2][0] = 1;

    KDTree kdTree(1, points);

    std::vector<double> p(1, 0.2);
    std::vector<std::vector<double> > neighbors;
    kdTree.findNearestNeighbors(p, 2, &neighbors);

    res = 1;
    expected = 1;
    subTestName = "simple_1_d";

    if(neighbors[0][0] != 0)
    {
        output_screen("FAIL! The first neighbor is wrong!");
        res = 0;
    }
    if(neighbors[1][0] != 1)
    {
        output_screen("FAIL! The second neighbor is wrong!");
        res = 0;
    }
}

void
TestKDTree::runSubTest2(double& res, double& expected, std::string& subTestName)
{
    std::vector<std::vector<double> > points(3);
    for(int i = 0; i < 3; ++i)
        points[i].resize(1, i);

    KDTree kdTree(1, points);
    std::vector<double> p(1, -1);
    kdTree.insert(p);
    p[0] = -2;
    kdTree.insert(p);

    p[0] = -5;

    std::vector<std::vector<double> > neighbors;
    kdTree.findNearestNeighbors(p, 4, &neighbors);

    res = 1;
    expected = 1;
    subTestName = "simple_1_d_insert";

    if(neighbors[0][0] != -2)
    {
        output_screen("FAIL! The first neighbor is wrong!");
        res = 0;
    }
    if(neighbors[1][0] != -1)
    {
        output_screen("FAIL! The second neighbor is wrong!");
        res = 0;
    }
    if(neighbors[2][0] != 0)
    {
        output_screen("FAIL! The third neighbor is wrong!");
        res = 0;
    }
    if(neighbors[3][0] != 1)
    {
        output_screen("FAIL! The fourth neighbor is wrong!");
        res = 0;
    }
}

void
TestKDTree::runSubTest3(double& res, double& expected, std::string& subTestName)
{
    std::vector<std::vector<double> > points;

    for(int i = -100; i < 100; ++i)
    {
        for(int j = -100; j < 100; ++j)
        {
            int n = points.size();
            points.resize(n + 1);

            points[n].resize(2);
            points[n][0] = double(i);
            points[n][1] = double(j);
        }
    }

    KDTree kdTree(2, points);

    std::vector<double> q(2);
    q[0] = 50.1;
    q[1] = 20.2;

    std::vector<unsigned long> indices;
    std::vector<double> distances;

    kdTree.findNearestNeighbors(q, 3, &indices, &distances);

    res = 1;
    expected = 1;
    subTestName = "2d_grid";

    if(points[indices[0]][0] != 50 || points[indices[0]][1] != 20)
    {
        output_screen("FAIL: First neighbor should be (50, 20) but it is (" << points[indices[0]][0] << ", " << points[indices[0]][1] << ")." << std::endl);
        res = 0;
    }

    if(points[indices[1]][0] != 50 || points[indices[1]][1] != 21)
    {
        output_screen("FAIL: Second neighbor should be (50, 21) but it is (" << points[indices[1]][0] << ", " << points[indices[1]][1] << ")." << std::endl);
        res = 0;
    }

    if(points[indices[2]][0] != 51 || points[indices[2]][1] != 20)
    {
        output_screen("FAIL: Third neighbor should be (51, 20) but it is (" << points[indices[2]][0] << ", " << points[indices[2]][1] << ")." << std::endl);
        res = 0;
    }

    if(!Math::areEqual(distances[0], 0.05, 1e-7))
    {
        output_screen("FAIL: Distance squared to the first neighbor should be " << 0.05 << " but it is " << distances[0] << "." << std::endl);
        res = 0;
    }
}

void
TestKDTree::runSubTest4(double& res, double& expected, std::string& subTestName)
{
    std::vector<std::vector<double> > points;

    for(int i = -50; i < 50; ++i)
    {
        for(int j = -50; j < 50; ++j)
        {
            for(int k = -50; k < 50; ++k)
            {
                int n = points.size();
                points.resize(n + 1);

                points[n].resize(3);
                points[n][0] = double(i);
                points[n][1] = double(j);
                points[n][2] = double(k);
            }
        }
    }

    res = 1;
    expected = 1;
    subTestName = "timer";

    Timer t1("KD TREE CONSTRUCTION");
    t1.start();
    KDTree kdTree(3, points);
    const unsigned long timeConstr = t1.end();

    if(timeConstr > 60000000)
    {
        output_screen("FAIL! KD tree construction should take only a few seconds, 1 minute max. It took " << timeConstr / 1000000 << " seconds!");
        res = 0;
    }

    std::vector<double> q(3, 0);

    std::vector<std::vector<double> > neighbors;
    std::vector<double> distances;

    Timer t2("KD TREE FIND NEAREST NEIGHBORS");
    t2.start();
    kdTree.findNearestNeighbors(q, 8, &neighbors, &distances);
    const unsigned long timeSearch = t2.end();
    if(timeSearch > 1000)
    {
        output_screen("FAIL! nearest neighbor search should take about 100 microseconds, 1000 max. It took " << timeSearch << " microseconds!");
        res = 0;
    }

    if(neighbors[0][0] != 0 || neighbors[0][1] != 0 || neighbors[0][2] != 0)
    {
        output_screen("FAIL: First neighbor should be (0, 0, 0) but it is (" << neighbors[0][0] << ", " << neighbors[0][1] << ", " << neighbors[0][2] << ")." << std::endl);
        res = 0;
    }

    for(int i = 1; i <= 6; ++i)
    {
        if(!Math::areEqual(distances[i], 1.0, 1e-7))
        {
            output_screen("FAIL! Neighbor " << i << " should be distance 1 away but it is " << distances[i] << "." << std::endl);
            res = 0;
        }
    }

    if(!Math::areEqual(distances[7], 2.0, 1e-7))
    {
        output_screen("FAIL! Neighbor 7 should be distance 2 away but it is " << distances[7] << "." << std::endl);
        res = 0;
    }
}

bool
TestKDTree::test(int dim, unsigned long nPoints, int k, int seed)
{
    check(dim > 0, "");
    check(k > 0, "");
    check(nPoints > 0, "");
    check(k <= nPoints, "");

    if(!seed)
        seed = std::time(0);

    Math::UniformRealGenerator gen(seed, -1, 1);

    std::vector<std::vector<double> > points(nPoints);
    for(unsigned long i = 0; i < nPoints; ++i)
    {
        points[i].resize(dim);
        for(int j = 0; j < dim; ++j)
            points[i][j] = gen.generate();
    }

    Timer t0("KD TREE CONSTRUCTION");
    t0.start();
    KDTree kdTree(dim, points);
    t0.end();

    std::vector<double> p(dim);
    for(int i = 0; i < dim; ++i)
        p[i] = gen.generate();

    Timer t1("KD TREE FIND NEAREST NEIGHBORS");
    t1.start();
    std::vector<unsigned long> indices;
    kdTree.findNearestNeighbors(p, k, &indices);
    t1.end();

    Timer t2("NEAREST NEIGHBORS BY SORT");
    std::vector<std::pair<double, unsigned long> > v(nPoints);
    for(unsigned long i = 0; i < nPoints; ++i)
    {
        double d = 0;
        for(int j = 0; j < dim; ++j)
        {
            const double x = p[j] - points[i][j];
            d += x * x;
            v[i].first = d;
            v[i].second = i;
        }
    }

    t2.start();
    std::sort(v.begin(), v.end());
    t2.end();

    check(indices.size() == k, "");
    for(int i = 0; i < k; ++i)
    {
        if(indices[i] != v[i].second)
            return false;
    }
    return true;
}

