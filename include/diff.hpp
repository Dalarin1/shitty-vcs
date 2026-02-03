#pragma once
#include <fstream>
#include <vector>
#include <algorithm>

enum class EditType : uint8_t
{
    Match = 0,
    Insert = 1,
    Delete = 2
};
struct DiffOp
{
    EditType type;
    uint32_t pos;
    uint8_t byte;
};

std::vector<DiffOp> myers_diff(const std::vector<char> &a, const std::vector<char> &b)
{
    const size_t N = a.size(), M = b.size(), MAX = N + M, OFFSET = MAX;
    std::vector<int> v(2 * MAX + 1, 0);
    struct Step
    {
        int x, y, k;
    };
    std::vector<Step> path;

    int d, k, x, y;
    for (d = 0; d <= MAX; ++d)
    {
        for (k = -d; k <= d; k += 2)
        {
            if (k == -d || (k != d && v[OFFSET + k - 1] < v[OFFSET + k + 1]))
                x = v[OFFSET + k + 1];
            else
                x = v[OFFSET + k - 1] + 1;
            y = x - k;
            while (x < N && y < M && a[x] == b[y])
            {
                x++;
                y++;
            }
            v[OFFSET + k] = x;
            path.push_back({x, y, k});
            if (x >= N && y >= M)
                goto end;
        }
    }
end:

    std::vector<DiffOp> ops;
    x = N;
    y = M;
    for (int i = path.size() - 1; i >= 0; --i)
    {
        auto [px, py, pk] = path[i];
        if (px == x && py == y)
            continue;
        while (x > px && y > py)
        {
            x--;
            y--;
            ops.push_back({EditType::Match, (uint32_t)x, (uint8_t)a[x]});
        }
        if (x > px)
        {
            x--;
            ops.push_back({EditType::Delete, (uint32_t)x, (uint8_t)a[x]});
        }
        else if (y > py)
        {
            y--;
            ops.push_back({EditType::Insert, (uint32_t)x, (uint8_t)b[y]});
        }
    }
    std::reverse(ops.begin(), ops.end());
    return ops;
}

void apply_diff(const std::vector<char> &base, const std::vector<DiffOp> &diff, std::vector<char> &out)
{
    out.clear();
    out.reserve(base.size() + diff.size());
    size_t pos = 0;
    for (auto &op : diff)
    {
        switch (op.type)
        {
        case EditType::Match:
            if (pos < base.size())
                out.push_back(base[pos++]);
            break;
        case EditType::Insert:
            out.push_back((char)op.byte);
            break;
        case EditType::Delete:
            if (pos < base.size())
                pos++;
            break;
        }
    }
    while (pos < base.size())
        out.push_back(base[pos++]);
}

void write_diff(const std::string &fn, const std::vector<DiffOp> &diff)
{
    std::ofstream f(fn, std::ios::binary);
    for (auto &o : diff)
    {
        uint8_t t = (uint8_t)o.type;
        f.write((char *)&t, 1);
        f.write((char *)&o.pos, 4);
        f.write((char *)&o.byte, 1);
    }
}
std::vector<DiffOp> read_diff(const std::string &fn)
{
    std::ifstream f(fn, std::ios::binary);
    std::vector<DiffOp> d;
    uint8_t t, b;
    uint32_t p;
    while (f.read((char *)&t, 1))
    {
        f.read((char *)&p, 4);
        f.read((char *)&b, 1);
        d.push_back({(EditType)t, p, b});
    }
    return d;
}
