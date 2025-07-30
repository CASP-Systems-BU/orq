#include "../include/secrecy.h"

using namespace secrecy::debug;
using namespace secrecy::service;
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

template <typename T>
EncodedTable<T> create_test_table(int column_count) {

    Vector<T> column_data = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::vector<Vector<T>> columns(column_count, column_data);

    std::vector<std::string> schema;
    for (int i = 1; i <= column_count; ++i) {
        schema.push_back("Column" + std::to_string(i));
    }

    EncodedTable<T> table = secret_share<T>(columns, schema);

    return table;
}

template <typename T>
void assert_table_schema(EncodedTable<T> table, size_t column_count) {
    auto table_open = table.open_with_schema();
    auto data_table = table_open.first;
    auto column_names = table_open.second;
    ASSERT_SAME(data_table.size(), column_count);
    ASSERT_SAME(column_names.size(), column_count);
}

template <typename T>
void test_add_columns() {
    single_cout_nonl("Testing " << std::numeric_limits<std::make_unsigned_t<T>>::digits 
                                << "-bit: addColumns... ");

    EncodedTable<T> table = create_test_table<T>(1);

    // Add single column
    table.addColumns({"Column2"}, table.size());
    assert_table_schema(table, 2);

    // Add multiple columns
    table.addColumns({"Column3", "Column4", "Column5", "Column6"}, table.size());
    assert_table_schema(table, 6);

    single_cout("OK");
}

template <typename T>
void test_delete_columns() {
    single_cout_nonl("Testing " << std::numeric_limits<std::make_unsigned_t<T>>::digits 
                                << "-bit: deleteColumns... ");

    EncodedTable<T> table = create_test_table<T>(6);

    // Delete single column
    table.deleteColumns({"Column2"});
    assert_table_schema(table, 5);

    // Delete multiple columns
    table.deleteColumns({"Column3", "Column4", "Column5", "Column6"});
    assert_table_schema(table, 1);

    // Delete non-existent column
    table.deleteColumns({"InvalidColumn"});
    assert_table_schema(table, 1);

    single_cout("OK");
}

template <typename T>
void test_project() {
    single_cout_nonl("Testing " << std::numeric_limits<std::make_unsigned_t<T>>::digits 
                                << "-bit: project... ");

    // Project single column
    {
        EncodedTable<T> table = create_test_table<T>(6);
        table.project({"Column1"});
        assert_table_schema(table, 1);
    }

    // Project multiple columns
    {
        EncodedTable<T> table = create_test_table<T>(6);
        table.project({"Column1", "Column2", "Column3", "Column4"});
        assert_table_schema(table, 4);
    }

    single_cout("OK");
}

template <typename T>
void test_resize() {
    single_cout_nonl("Testing " << std::numeric_limits<std::make_unsigned_t<T>>::digits 
                                << "-bit: resize... ");

    const size_t n_rows = 10000;
    EncodedTable<T> table = secret_share<T>({Vector<T>(n_rows, 99), Vector<T>(n_rows, 77)}, {"A", "B"});

    assert(table.size() == n_rows);

    // Get a reference to the first column to check later
    auto V = (ASharedVector<T>*)table["A"].contents.get();

    const size_t n_head = 8000;
    const size_t n_tail = 3000;
    const size_t n_resize_down = 2000;

    table.head(n_head);
    assert(table.size() == n_head);

    table.tail(n_tail);
    assert(table.size() == n_tail);

    table.resize(n_resize_down);
    assert(table.size() == n_resize_down);

    table.pad_power_of_two();
    ASSERT_SAME(table.size(), 1 << std::bit_width(n_resize_down));

    // Check that operators actually resized underlying storage
    assert(V->size() == table.size());

    // Confirm value hasn't been changed
    assert(V->open()[0] == 99);

    // Change the *table* value
    table["A"].zero();

    // ...which should reflect in the underlying vector
    // (if resize operators are making new tables this test will fail)
    assert(V->open()[0] == 0);

    single_cout("OK");
}


int main(int argc, char** argv) {
    // Initialize Secrecy runtime
    secrecy_init(argc, argv);

    // Testing basic table operations
    test_add_columns<int>();
    test_delete_columns<int>();
    test_project<int>();
    test_resize<int>();

    MPI_Finalize();
    return 0;
}