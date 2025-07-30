#ifndef SECRECY_BENCHMARKING_PRIMITIVES_H
#define SECRECY_BENCHMARKING_PRIMITIVES_H

// printf("%d _op_ %d = %d or %d\n", x[i], y[i], x[i] _op_ y[i], z[i]);                         
#define define_primitive_benchmarking_2(_op_, _ra_, _rb_, _x_, _y_, _z_, _name_1, _name_2, _name_3, _get_func);     \
private:                                                                                                            \
static void _name_1(ShareTable *_table) {                                                                           \
    ShareTable &table = *_table;                                                                                    \
    table._get_func(_z_) = table._get_func(_x_) _op_ table._get_func(_y_);                                          \
}                                                                                                                   \
                                                                                                                    \
                                                                                                                    \
static void _name_2(DataTable *_table) {                                                                            \
    DataTable &table = *_table;                                                                                     \
                                                                                                                    \
    int size = table[0].size();                                                                                     \
    auto &x = table[0];                                                                                             \
    auto &y = table[1];                                                                                             \
    auto &z = table[2];                                                                                             \
                                                                                                                    \
    for (int i = 0; i < size; ++i) {                                                                                \
        z[i] = x[i] _op_ y[i];                                                                                      \
    }                                                                                                               \
}                                                                                                                   \
                                                                                                                    \
public:                                                                                                             \
static void _name_3(const int &batches_num, const int &batch_size) {                                                \
    BenchMark::timedExecute(batch_size, batches_num, #_name_3,                                                      \
                            _name_1, _name_2,                                                                       \
                            #_name_3, {_x_, _y_, _z_}, batch_size,                                                  \
                            _ra_ * batch_size, _rb_ * batch_size);                                                  \
}                                                                                                                   \
                                                                                                                    \


namespace secrecy { namespace benchmarking {

    template<typename ShareTable, typename DataTable, typename BenchMark>
    class primitives {
        define_primitive_benchmarking_2(&, 0, 1, "[x]", "[y]", "[z]", mpc_and_b, plain_and_b, benchmark_and_b, get_b);
        define_primitive_benchmarking_2(|, 0, 1, "[x]", "[y]", "[z]", mpc_or_b, plain_or_b, benchmark_or__b, get_b);
        define_primitive_benchmarking_2(^, 0, 0, "[x]", "[y]", "[z]", mpc_xor_b, plain_xor_b, benchmark_xor_b, get_b);

        define_primitive_benchmarking_2(>, 0, 7, "[x]", "[y]", "[z]", mpc_gt_b, plain_gt_b, benchmark_gt__b, get_b);
        define_primitive_benchmarking_2(<, 0, 7, "[x]", "[y]", "[z]", mpc_lt_b, plain_lt_b, benchmark_lt__b, get_b);
        define_primitive_benchmarking_2(==, 0, 6, "[x]", "[y]", "[z]", mpc_eq_b, plain_eq_b, benchmark_eq__b, get_b);
        define_primitive_benchmarking_2(!=, 0, 6, "[x]", "[y]", "[z]", mpc_neq_b, plain_neq_b, benchmark_neq_b, get_b);
        define_primitive_benchmarking_2(>=, 0, 7, "[x]", "[y]", "[z]", mpc_gte_b, plain_gte_b, benchmark_gte_b, get_b);
        define_primitive_benchmarking_2(<=, 0, 7, "[x]", "[y]", "[z]", mpc_lte_b, plain_lte_b, benchmark_lte_b, get_b);

        define_primitive_benchmarking_2(+, 0, 0, "x", "y", "z", mpc_add_a, plain_add_a, benchmark_add_a, get_a);
        define_primitive_benchmarking_2(-, 0, 0, "x", "y", "z", mpc_sub_a, plain_sub_a, benchmark_sub_a, get_a);
        define_primitive_benchmarking_2(*, 1, 0, "x", "y", "z", mpc_mul_a, plain_mul_a, benchmark_mul_a, get_a);
    };

} } //namespace secrecy::benchmarking::primitives


#endif //SECRECY_BENCHMARKING_PRIMITIVES_H
