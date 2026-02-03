#include <fstream>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cctype>

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
    const int N = static_cast<int>(a.size());
    const int M = static_cast<int>(b.size());
    const int MAX = N + M;
    const int OFFSET = MAX;

    std::vector<int> v(2 * MAX + 1, 0);
    std::vector<std::vector<int>> trace;

    int D;
    for (D = 0; D <= MAX; ++D)
    {
        std::vector<int> v_copy = v;
        for (int k = -D; k <= D; k += 2)
        {
            int x;
            if (k == -D || (k != D && v[OFFSET + k - 1] < v[OFFSET + k + 1]))
                x = v[OFFSET + k + 1];
            else
                x = v[OFFSET + k - 1] + 1;

            int y = x - k;

            while (x < N && y < M && a[x] == b[y])
            {
                ++x;
                ++y;
            }

            v_copy[OFFSET + k] = x;
        }

        trace.push_back(v_copy);
        v.swap(v_copy);

        if (v[OFFSET + (N - M)] >= N && (v[OFFSET + (N - M)] - (N - M)) >= M)
            break;
    }

    int x = N;
    int y = M;
    std::vector<DiffOp> ops;

    for (int d = (int)trace.size() - 1; d >= 0; --d)
    {
        if (d == 0)
            break;

        const auto &v_prev = trace[d - 1];
        int k = x - y;

        int prev_k;
        bool down;
        if (k == -d || (k != d && v_prev[OFFSET + k - 1] < v_prev[OFFSET + k + 1]))
        {
            prev_k = k + 1;
            down = true; // вставка
        }
        else
        {
            prev_k = k - 1;
            down = false; // удаление
        }

        int prev_x = v_prev[OFFSET + prev_k];
        int prev_y = prev_x - prev_k;

        while (x > prev_x && y > prev_y)
        {
            --x;
            --y;
            ops.push_back({EditType::Match, static_cast<uint32_t>(x), static_cast<uint8_t>(a[x])});
        }

        if (down)
        {
            --y;
            ops.push_back({EditType::Insert, static_cast<uint32_t>(x), static_cast<uint8_t>(b[y])});
        }
        else
        {
            --x;
            ops.push_back({EditType::Delete, static_cast<uint32_t>(x), static_cast<uint8_t>(a[x])});
        }
    }

    // добираем оставшиеся совпадения (если строка пустая)
    while (x > 0 && y > 0)
    {
        --x;
        --y;
        ops.push_back({EditType::Match, static_cast<uint32_t>(x), static_cast<uint8_t>(a[x])});
    }

    // если остались только вставки
    while (y > 0)
    {
        --y;
        ops.push_back({EditType::Insert, 0, static_cast<uint8_t>(b[y])});
    }

    // если остались только удаления
    while (x > 0)
    {
        --x;
        ops.push_back({EditType::Delete, static_cast<uint32_t>(x), static_cast<uint8_t>(a[x])});
    }

    std::reverse(ops.begin(), ops.end());
    return ops;
}
void apply_diff(const std::vector<char> &base, const std::vector<DiffOp> &diff, std::vector<char> &out)
{
    out.clear();
    size_t base_pos = 0;

    for (const auto &op : diff)
    {
        // Добавляем всё, что идёт до позиции операции
        while (base_pos < op.pos && base_pos < base.size())
        {
            out.push_back(base[base_pos++]);
        }

        switch (op.type)
        {
        case EditType::Insert:
            out.push_back(static_cast<char>(op.byte));
            break;

        case EditType::Delete:
            if (base_pos < base.size())
                base_pos++;
            break;

        case EditType::Match:
            if (base_pos < base.size())
                out.push_back(base[base_pos++]);
            break;
        }
    }

    // Добавляем остаток исходного файла
    while (base_pos < base.size())
        out.push_back(base[base_pos++]);
}
// void apply_diff(const std::vector<char> &base, const std::vector<DiffOp> &diff, std::vector<char> &out)
// {
//     out.clear();
//     out.reserve(base.size() + diff.size());
//     size_t pos = 0;
//     for (auto &op : diff)
//     {
//         switch (op.type)
//         {
//         case EditType::Match:
//             if (pos < base.size())
//                 out.push_back(base[pos++]);
//             break;
//         case EditType::Insert:
//             out.push_back((char)op.byte);
//             break;
//         case EditType::Delete:
//             if (pos < base.size())
//                 pos++;
//             break;
//         }
//     }
//     while (pos < base.size())
//         out.push_back(base[pos++]);
// }

// void apply_diff(const std::vector<char> &base, const std::vector<DiffOp> &diff, std::vector<char> &out)
// {
//     out.clear();
//     size_t base_pos = 0;

//     for (const auto &op : diff)
//     {
//         // Копируем все байты из base до текущей позиции операции
//         while (base_pos < op.pos && base_pos < base.size())
//         {
//             out.push_back(base[base_pos++]);
//         }

//         switch (op.type)
//         {
//         case EditType::Insert:
//             out.push_back(static_cast<char>(op.byte));
//             break;

//         case EditType::Delete:
//             if (base_pos < base.size())
//                 base_pos++;
//             break;

//         case EditType::Match:
//             if (base_pos < base.size())
//                 out.push_back(base[base_pos++]);
//             break;
//         }
//     }

//     // Добавляем остаток исходного файла
//     while (base_pos < base.size())
//     {
//         out.push_back(base[base_pos++]);
//     }
// }
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
#ifdef _WIN32
#include <windows.h>
void enable_vt_mode() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#endif
#define ANSI_RESET   "\033[0m"
#define ANSI_RED     "\033[31m"
#define ANSI_GREEN   "\033[32m"
#define ANSI_GRAY    "\033[90m"

void print_diff_colored(const std::vector<DiffOp> &diff)
{
    std::cout << "---- DIFF START ----\n";
    for (const auto &op : diff)
    {
        switch (op.type)
        {
        case EditType::Insert:
            std::cout << ANSI_GREEN << "+ "
                      << (isprint(op.byte) ? char(op.byte) : '.')
                      << " (pos " << op.pos << ")" << ANSI_RESET << "\n";
            break;
        case EditType::Delete:
            std::cout << ANSI_RED << "- "
                      << (isprint(op.byte) ? char(op.byte) : '.')
                      << " (pos " << op.pos << ")" << ANSI_RESET << "\n";
            break;
        case EditType::Match:
            std::cout << ANSI_GRAY << "  "
                      << (isprint(op.byte) ? char(op.byte) : '.')
                      << " (pos " << op.pos << ")" << ANSI_RESET << "\n";
            break;
        }
    }
    std::cout << "---- DIFF END ----\n";
}
