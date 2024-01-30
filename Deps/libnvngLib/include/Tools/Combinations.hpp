#include <vector>

class Combinations
{
public :
        typedef std::vector<int64_t> combination_t;

        // initialize status
        Combinations(int64_t N, int64_t R) :
                completed(N < 1 || R > N),
                generated(0),
                N(N), R(R)
        {
                curr.reserve(R);
                for (int c = 1; c <= R; ++c)
                        curr.push_back(c);
        }

        // true while there are more solutions
        bool completed = false;

        // count how many generated
        int64_t generated = 0;

        // get current and compute next combination
        combination_t next()
        {
                combination_t ret = curr;

                // find what to increment
                completed = true;
                for (int i = R - 1; i >= 0; --i)
                {
                        if (curr[i] < N - R + i + 1)
                        {
                                int j = curr[i] + 1;
                                while (i <= R - 1)
                                        curr[i++] = j++;
                                completed = false;
                                ++generated;
                                break;
                        }
                }

                return ret;
        }

private :
        const int64_t N = 0;
        const int64_t R = 0;
        combination_t curr;
};
