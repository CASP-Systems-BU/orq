/**
 * @file tpch_dbgen.h
 * @brief TPCH Database Generator
 * 
 * Reference: Ch. 1 of the TPC-H Specification,
 * https://www.tpc.org/TPC_Documents_Current_Versions/pdf/TPC-H_v3.0.1.pdf
 * 
 * Naming semantics:
 * - Table names are all capitalized
 * - Column names should use PascalCase
 * - Column names should not use table prefixes, unless required for
 *   disambiguation in a query. In that case, a single-character table identifer
 *   can be prepended, with an underscore (e.g., `O_Comment`)
 */

#include "../../../include/secrecy.h"

#include <numeric>
#include <bit>
#include <sqlite3.h>
#include <sstream>

using namespace secrecy::debug;
using namespace secrecy::service;

using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

#ifdef SECRECY_QUERY
// Secrecy requires power-of-2 input, so Customers table is 8x smaller than
// Orders table.
#define CUSTOMERS_MULTIPLIER (187500)
#else
// Proper TPCH has it 10x smaller (150k)
#define CUSTOMERS_MULTIPLIER (150 * 1000)
#endif

#define ORDERS_MULTIPLIER (1500 * 1000)

#define PART_MULTIPLIER (200 * 1000)
#define SUPPLIER_MULTIPLIER (10 * 1000)
#define PARTSUPP_MULTIPLIER (800 * 1000)
#define NATION_SIZE (25)
#define REGION_SIZE (5)

#define PARTSUPP_PER_PART 4

// Only 2/3 of customers should have orders
#define CUSTOMER_ORDER_FRACTION (2.0 / 3)

#define COMMENT_BITS 4


// Proactive sharing for Q6 - defaults
#ifndef TPCH_DATE_THRESHOLD
#define TPCH_DATE_THRESHOLD 100
#endif
#ifndef TPCH_DATE_INTERVAL
#define TPCH_DATE_INTERVAL 10
#endif
#ifndef TPCH_DISCOUNT_THRESHOLD
#define TPCH_DISCOUNT_THRESHOLD 5
#endif
#ifndef TPCH_QUANTITY_THRESHOLD
#define TPCH_QUANTITY_THRESHOLD 40
#endif


template <typename T = int>
class TPCDatabase
{
protected:
    /**
     * @brief Generate a random vector of type T (possibly signed) from [min_val,
     * max_val).
     * 
     * @param size 
     * @param min_val the minimum value (inclusive)
     * @param max_val the maximum value (*exclusive*)
     * @return Vector<T> 
     */
    Vector<T> randomColumn(size_t size, T min_val, T max_val)
    {
        using uT = std::make_unsigned_t<T>;

        Vector<uT> uvec(size);
        runTime->populateLocalRandom(uvec);

        auto range = max_val - min_val;

        Vector<T> r(size);
        r = (uvec % range) + min_val;
        return r;
    }

    /**
     * @brief Generate a counter column, suitable for primary keys, etc. Will
     * count from 0 to size - 1.
     * 
     * @param size 
     * @return Vector<T> 
     */
    Vector<T> counterColumn(size_t size)
    {
        Vector<T> r(size);
        std::iota(r.begin(), r.end(), 0);
        return r;
    }

public:
    const double scaleFactor;

    // SQLite DB for output validation
    sqlite3 *sqlite_db;

    size_t lineItemsSize = 0;

    TPCDatabase(double sf) : scaleFactor(sf), sqlite_db(nullptr) {}

    TPCDatabase(double sf, sqlite3 *sqlite_db) : scaleFactor(sf), sqlite_db(sqlite_db) {}

    size_t customersSize()
    {
        return std::round(scaleFactor * CUSTOMERS_MULTIPLIER);
    }

    size_t ordersSize()
    {
        return std::round(scaleFactor * ORDERS_MULTIPLIER);
    }

    size_t lineitemsSize()
    {
        if (! lineItemsSize) {
            throw new std::runtime_error("LineItems not yet generated!");
        }
        return lineItemsSize;
    }

    size_t partSize() {
        return std::round(scaleFactor * PART_MULTIPLIER);
    }

    size_t partSuppSize() {
        return std::round(scaleFactor * PARTSUPP_MULTIPLIER);
    }

    size_t supplierSize() {
        return std::round(scaleFactor * SUPPLIER_MULTIPLIER);
    }

    size_t nationSize() {
        return NATION_SIZE;
    }

    size_t regionSize() {
        return REGION_SIZE;
    }

    EncodedTable<T> getCustomersTable()
    {
        auto S = customersSize();

        // C_CUSTKEY unique within [SF * 150,000]
        auto custkey = counterColumn(S);

        // Integer representation for name
        auto name = custkey;

        // Foreign key to Nation
        auto nationKey = randomColumn(S, 0, nationSize());

        // C_ADDRESS random v-string [10,40]
        // We'll represent this as a random integer for simplicity
        auto address = randomColumn(S, 0, S);

        // C_PHONE generated according to Clause 4.2.2.9
        // We'll represent this as a random integer for simplicity
        auto phone = randomColumn(S, 1000000, 9999999);

        // C_CNTRYCODE, based on C_PHONE.
        // This is a preprocessing step to get the country code from the phone number.
        auto cntrycode = phone / 100000;

        // C_ACCTBAL random value [-999.99 .. 9,999.99]
        // We'll multiply by 100 to store as integer
        auto acctbal = randomColumn(S, -99999, 999999 + 1);

        // C_MKTSEGMENT random string [Segments]
        // We'll represent this as an integer 1-5
        auto mktsegment = randomColumn(S, 1, 6);

        // C_COMMENT text string [29,116]
        auto comment = randomColumn(S, 0, (1 << COMMENT_BITS));

        if (sqlite_db) {
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE CUSTOMER (
                    CustKey INTEGER, Name INTEGER, Address INTEGER, NationKey INTEGER, Phone INTEGER,
                    CntryCode INTEGER, AcctBal INTEGER, MktSegment INTEGER, Comment INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Customer (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions in SQLite
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO CUSTOMER (
                    CustKey, Name, Address, NationKey, Phone, CntryCode, AcctBal, MktSegment, Comment)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, custkey[i]);
                sqlite3_bind_int(stmt, 2, name[i]);
                sqlite3_bind_int(stmt, 3, address[i]);
                sqlite3_bind_int(stmt, 4, nationKey[i]);
                sqlite3_bind_int(stmt, 5, phone[i]);
                sqlite3_bind_int(stmt, 6, cntrycode[i]);
                sqlite3_bind_int(stmt, 7, acctbal[i]);
                sqlite3_bind_int(stmt, 8, mktsegment[i]);
                sqlite3_bind_int(stmt, 9, comment[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Customer (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_customer_custkey ON CUSTOMER(CustKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Customer (Index): " << sqlite3_errmsg(sqlite_db))
            }

            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Customer (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        std::vector<std::string> schema = {
            "[CustKey]", "[C_Name]", "[Address]", "[NationKey]", "[Phone]",
            "[CntryCode]", "[AcctBal]", "AcctBal", "[MktSegment]", "[Comment]"
        };

        EncodedTable<T> table = secret_share<T>(
            {custkey, name, address, nationKey, phone, cntrycode, acctbal, acctbal, mktsegment, comment}, 
            schema
        );

        table.tableName = "CUSTOMER";

        return table;
    }

    /**
     * @brief Get the Lineitem Table object
     * 
     * @param proactive_sharing whether to use proactive sharing: also return
     * in the table secret-shared values with constant thresholds subtracted off
     * already.
     * 
     * @return EncodedTable<T> 
     */
    EncodedTable<T> getLineitemTable(bool proactive_sharing = false) {
        // Slightly convoluted generation to handle randomly-sized lineItems
        // table. P0 generates size, then sends to others.
        // The approximate size is 4 * ordersSize, but that's not exact. We also
        // could just fix the size.

        auto pid = runTime->getPartyID();
        Vector<T> lineitem_per_order(ordersSize());
#ifndef MPC_PROTOCOL_DUMMY_ZERO
        if (pid == 0) {
            // random number [1..7] lineitems per order
            lineitem_per_order = randomColumn(ordersSize(), 1, 7 + 1);
            // generate indices
            lineitem_per_order.prefix_sum();

            // get last index, + 1
            lineItemsSize = lineitem_per_order[ordersSize() - 1] + 1;

            for (int p = 1; p < runTime->getNumParties(); p++) {
                runTime->comm0()->sendShare((int64_t) lineItemsSize, p);
            }
        } else {
            runTime->comm0()->receiveShare((int64_t&) lineItemsSize, -pid);
        }

        size_t S = lineItemsSize;
#else
        size_t S = 4 * ordersSize();
#endif
        Vector<T> orderKey(S);

        if (pid == 0) {
            for (int i = 0; i < ordersSize(); i++) {
                orderKey[lineitem_per_order[i]] = 1;
            }
            orderKey.prefix_sum();
        }

        // This will be [0..6] repeated, so no order (which is at most 7
        // lineitems) will have a duplicate. Specific value not too important.
        auto lineNumber = counterColumn(S) % 7;

        // Foreign key to Supplier
        auto suppKey = randomColumn(S, 0, supplierSize());

        auto returnFlag = randomColumn(S, 0, 3);
        auto lineStatus = randomColumn(S, 0, 2);
        auto quantity = randomColumn(S, 1, 51);

        // extendedPrice is defined as quantity * retailPrice
        // retailPrice (in parts table) is approximately random with range 90k
        // to 111k. Here, just make it actually random, and to prevent overflow,
        // represent as thousands of dollars. Then multiply by quantity.
        auto extendedPrice = randomColumn(S, 90, 111);
        extendedPrice *= quantity;

        // This should be a decimal (0% to 10%; repr. 0 to 0.1) but until we add
        // fixed point numbers, use integers. Queries will divide by 100 where
        // necessary.
        auto discount = randomColumn(S, 0, 10);

        // same thing here
        auto tax = randomColumn(S, 0, 8);

        // just make this random; ignore order date
        // for now, pretend orderdate => t=0
        // TODO: o_orderdate + [1..121]
        // (this would have to be a FK lookup)
        auto shipDate = randomColumn(S, 1, 121);

        // same thing here
        // order date + [30, 90]
        auto commitDate = randomColumn(S, 30, 90);
        // ship date + [30, 90]
        auto receiptDate = shipDate + randomColumn(S, 1, 30);

        // Enum of 7 modes
        auto shipMode = randomColumn(S, 0, 7);

        // Foreign key to Part
        auto partKey = randomColumn(S, 0, partSize());

        // Enum of 4 instructions
        auto shipInstruct = randomColumn(S, 0, 4);

        if (sqlite_db) {
            // Create table in SQLite
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE LINEITEM (
                    OrderKey INTEGER, ReturnFlag INTEGER, LineStatus INTEGER,
                    Quantity INTEGER, ExtendedPrice INTEGER, Discount INTEGER,
                    Tax INTEGER, ShipDate INTEGER, CommitDate INTEGER,
                    ReceiptDate Integer, ShipMode INTEGER, PartKey Integer,
                    ShipInstruct INTEGER, LineNumber INTEGER, SuppKey INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on LineItem (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions in SQLite
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO LINEITEM (
                    OrderKey, ReturnFlag, LineStatus, Quantity, ExtendedPrice,
                    Discount, Tax, ShipDate, CommitDate, ReceiptDate, ShipMode,
                    PartKey, ShipInstruct, LineNumber, SuppKey)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, orderKey[i]);
                sqlite3_bind_int(stmt, 2, returnFlag[i]);
                sqlite3_bind_int(stmt, 3, lineStatus[i]);
                sqlite3_bind_int(stmt, 4, quantity[i]);
                sqlite3_bind_int(stmt, 5, extendedPrice[i]);
                sqlite3_bind_int(stmt, 6, discount[i]);
                sqlite3_bind_int(stmt, 7, tax[i]);
                sqlite3_bind_int(stmt, 8, shipDate[i]);
                sqlite3_bind_int(stmt, 9, commitDate[i]);
                sqlite3_bind_int(stmt, 10, receiptDate[i]);
                sqlite3_bind_int(stmt, 11, shipMode[i]);
                sqlite3_bind_int(stmt, 12, partKey[i]);
                sqlite3_bind_int(stmt, 13, shipInstruct[i]);
                sqlite3_bind_int(stmt, 14, lineNumber[i]);
                sqlite3_bind_int(stmt, 15, suppKey[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on LineItem (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_lineitem_partkey ON LINEITEM(PartKey);
                CREATE INDEX idx_lineitem_orderkey ON LINEITEM(OrderKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on LineItem (Index): " << sqlite3_errmsg(sqlite_db))
            }

            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on LineItem (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        std::vector<std::string> schema = {
            "[OrderKey]",    "[ReturnFlag]",  "[LineStatus]", "Quantity",  "[Quantity]",
            "ExtendedPrice", "Discount",      "[Discount]",   "Tax",       "[ShipDate]",
            "[CommitDate]",  "[ReceiptDate]", "[ShipMode]",   "[PartKey]", "[ShipInstruct]",
            "[LineNumber]",  "[SuppKey]"};

        std::vector<Vector<T>> data = {
            orderKey, returnFlag, lineStatus,   quantity,   quantity,   extendedPrice,
            discount, discount,   tax,          shipDate,   commitDate, receiptDate,
            shipMode, partKey,    shipInstruct, lineNumber, suppKey};

        if (proactive_sharing) {
            auto shipDateAdj = shipDate - TPCH_DATE_THRESHOLD;
            auto shipDateAdjInterval = shipDate - (TPCH_DATE_THRESHOLD + TPCH_DATE_INTERVAL);
            auto discountHigh = discount - (TPCH_DISCOUNT_THRESHOLD - 1);
            auto discountLow = discount - (TPCH_DISCOUNT_THRESHOLD + 2);  // +2 covers <= with <
            auto quantityAdj = quantity - TPCH_QUANTITY_THRESHOLD;

            std::vector<Vector<T>> proactive_data = {shipDateAdj, shipDateAdjInterval, discountHigh,
                                                     discountLow, quantityAdj};
            std::vector<std::string> proactive_schema = {"[ShipDateAdj]", "[ShipDateAdjInterval]",
                                                         "[DiscountHigh]", "[DiscountLow]",
                                                         "[QuantityAdj]"};
            data.insert(data.end(), proactive_data.begin(), proactive_data.end());
            schema.insert(schema.end(), proactive_schema.begin(), proactive_schema.end());
        }

        EncodedTable<T> table = secret_share<T>(data, schema);

        table.tableName = "LINEITEMS";
        return table;

    }

    EncodedTable<T> getOrdersTable()
    {
        auto S = ordersSize();

        std::vector<std::string> schema = {
            "[OrderKey]", "[CustKey]", "[Comment]",
            "[OrderPriority]", "[OrderDate]", "[TotalPrice]", "[OrderStatus]"
        };

        // primary key
        auto orderKey = counterColumn(S);

        // foreign key, set this up later.
        // Approximate bitmask.
        int max_key = int(std::round(customersSize() * CUSTOMER_ORDER_FRACTION));
        auto custKey = randomColumn(S, 0, max_key);

        // 4-bit comment
        auto comment = randomColumn(S, 0, (1 << COMMENT_BITS));

        // 1..5
        auto orderPriority = randomColumn(S, 1, 5 + 1);

        // 1..121
        auto orderDate = randomColumn(S, 1, 121);

        // Total Price is defined as sum(L_EXTENDEDPRICE * (1+L_TAX) * (1-L_DISCOUNT)) for all lineitems
        // of an order. Tax and Discount approximately cancel each other out.
        // Estimating based on the l_extendedprice range of 90 to 111
        auto totalPrice = randomColumn(S, 90, 111);

        // Technically based on LineItem; for now just randomize 0-1-2
        auto orderStatus = randomColumn(S, 0, 2 + 1);

        EncodedTable<T> table = secret_share<T>({
            orderKey, custKey, comment, orderPriority, orderDate, totalPrice, orderStatus},
            schema
        );

        if (sqlite_db) {
            // Create table
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE ORDERS (
                    OrderKey INTEGER, CustKey INTEGER, Comment INTEGER,
                    OrderPriority INTEGER, OrderDate INTEGER, TotalPrice INTEGER,
                    OrderStatus INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Orders (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO ORDERS (
                    OrderKey, CustKey, Comment,
                    OrderPriority, OrderDate, TotalPrice, OrderStatus)
                VALUES (?, ?, ?, ?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, orderKey[i]);
                sqlite3_bind_int(stmt, 2, custKey[i]);
                sqlite3_bind_int(stmt, 3, comment[i]);
                sqlite3_bind_int(stmt, 4, orderPriority[i]);
                sqlite3_bind_int(stmt, 5, orderDate[i]);
                sqlite3_bind_int(stmt, 6, totalPrice[i]);
                sqlite3_bind_int(stmt, 7, orderStatus[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Orders (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_orders_orderkey ON ORDERS(OrderKey);
                CREATE INDEX idx_orders_custkey ON ORDERS(CustKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Orders (Index): " << sqlite3_errmsg(sqlite_db))
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Orders (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "ORDERS";

        return table;
    }

    EncodedTable<T> getRegionTable() {
        auto S = regionSize();

        std::vector<std::string> schema = {
            "[RegionKey]", "[Name]", "[Comment]"
        };

        // Primary key
        auto regionKey = counterColumn(S);

        // Integer representation for name
        // Value calculation is arbitrary, it's solely to obtain unique values 
        // that differ from the primary key
        auto name = regionKey * 2 + 1;

        // 4-bit comment
        auto comment = randomColumn(S, 0, (1 << COMMENT_BITS));

        EncodedTable<T> table = secret_share<T>(
            {regionKey, name, comment},
            schema
        );

        if (sqlite_db) {
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE REGION (
                    RegionKey INTEGER, Name INTEGER, Comment INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Region (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO REGION (
                    RegionKey, Name, Comment)
                VALUES (?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < regionSize(); ++i) {
                sqlite3_bind_int(stmt, 1, regionKey[i]);
                sqlite3_bind_int(stmt, 2, name[i]);
                sqlite3_bind_int(stmt, 3, comment[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Region (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_region_regionkey ON REGION(RegionKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Region (Index): " << sqlite3_errmsg(sqlite_db))
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Region (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "REGION";
        return table;
    }

    EncodedTable<T> getNationTable() {
        auto S = nationSize();

        std::vector<std::string> schema = {
            "[NationKey]", "[Name]", "[RegionKey]", "[Comment]"
        };

        // Primary key
        auto nationKey = counterColumn(S);

        // Integer representation for name
        // Value calculation is arbitrary, it's solely to obtain unique values 
        // that differ from the primary key
        auto name = nationKey * 2 + 1;

        // nation regions
        Vector<T> regionKey = {0, 1, 1, 1, 4, 0, 3, 3, 2, 2, 4, 4, 2, 4, 0, 0, 0, 1, 2, 3, 4, 2, 3, 3, 1};
                
        // 4-bit comment
        auto comment = randomColumn(S, 0, (1 << COMMENT_BITS));

        if (sqlite_db) {
            // Create table
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE NATION (
                    NationKey INTEGER, Name INTEGER, 
                    RegionKey INTEGER, Comment INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Nation (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO NATION (
                    NationKey, Name, RegionKey, Comment)
                VALUES (?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < nationSize(); ++i) {
                sqlite3_bind_int(stmt, 1, nationKey[i]);
                sqlite3_bind_int(stmt, 2, name[i]);
                sqlite3_bind_int(stmt, 3, regionKey[i]);
                sqlite3_bind_int(stmt, 4, comment[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Nation (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_nation_nationkey ON NATION(NationKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Nation (Index): " << sqlite3_errmsg(sqlite_db))
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Nation (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        EncodedTable<T> table = secret_share<T>(
            {nationKey, name, regionKey, comment},
            schema
        );

        table.tableName = "NATION";
        return table;
    }
    
    EncodedTable<T> getPartTable() {
        auto S = partSize();

        std::vector<std::string> schema = {
            "[PartKey]", "[Brand]", "[Container]", "[Type]", "[Size]", "[Name]"
        };

        // primary key
        auto partkey = counterColumn(S);

        // 1..25
        auto brand = randomColumn(S, 1, 25 + 1);

        // 1..40
        auto container = randomColumn(S, 1, 40 + 1);

        // 1..10
        auto type = randomColumn(S, 1, 10 + 1);

        // 1..50
        auto size = randomColumn(S, 1, 50 + 1);

        // Spec defines this as a concatenation of 5 words from a list of ~100, using a range of 20
        // to get a similar representation for any one word
        auto name = randomColumn(S, 0, 20 + 1);

        EncodedTable<T> table = secret_share<T>({partkey, brand, container, type, size, name},
                                                schema);

        if (sqlite_db) {
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE PART (
                    PartKey INTEGER, Brand INTEGER, Container INTEGER,
                    Type INTEGER, Size INTEGER, Name INTEGER
                );
            )sql";
            char* errMsg = nullptr;
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, &errMsg);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Part (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO PART (PartKey, Brand, Container, Type, Size, Name)
                VALUES (?, ?, ?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < partSize(); ++i) {
                sqlite3_bind_int(stmt, 1, partkey[i]);
                sqlite3_bind_int(stmt, 2, brand[i]);
                sqlite3_bind_int(stmt, 3, container[i]);
                sqlite3_bind_int(stmt, 4, type[i]);
                sqlite3_bind_int(stmt, 5, size[i]);
                sqlite3_bind_int(stmt, 6, name[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Part (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_part_partkey ON PART(PartKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Part (Index): " << sqlite3_errmsg(sqlite_db))
            }

            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Part (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "PART";
        return table;
    }

    EncodedTable<T> getSupplierTable() {
        auto S = supplierSize();

        std::vector<std::string> schema = {
            "[SuppKey]", "[NationKey]", "[AcctBal]", "[Name]", "[Address]", "[Phone]", "[Comment]"
        };

        // Primary key
        auto suppkey = counterColumn(S);

        // Foreign key to Nation
        auto nationkey = randomColumn(S, 0, nationSize());

        // -1000..10000
        auto acctbal = randomColumn(S, -1000, 10000 + 1);

        // Integer representation for name
        // Value calculation is arbitrary, it's solely to obtain unique values
        // that differ from the primary key
        auto name = suppkey * 2 + 1;

        // 4-bit comment
        auto comment = randomColumn(S, 0, (1 << COMMENT_BITS));
        
        // Integer representation for address in 0..10000
        auto address = randomColumn(S, 0, 100000 + 1);

        // Integer representation for phone number in 1B..2B
        auto phone = randomColumn(S, 1000000000, 2000000000 + 1);

        EncodedTable<T> table = secret_share<T>(
            {suppkey, nationkey, acctbal, name, address, phone, comment},
            schema
        );

        if (sqlite_db) {
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE SUPPLIER (
                    SuppKey INTEGER, NationKey INTEGER,
                    AcctBal INTEGER, Name INTEGER, Address INTEGER, Phone INTEGER, Comment INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Supplier (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO SUPPLIER (
                    SuppKey, NationKey, AcctBal, Name, Address, Phone, Comment)
                VALUES (?, ?, ?, ?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < supplierSize(); ++i) {
                sqlite3_bind_int(stmt, 1, suppkey[i]);
                sqlite3_bind_int(stmt, 2, nationkey[i]);
                sqlite3_bind_int(stmt, 3, acctbal[i]);
                sqlite3_bind_int(stmt, 4, name[i]);
                sqlite3_bind_int(stmt, 5, address[i]);
                sqlite3_bind_int(stmt, 6, phone[i]);
                sqlite3_bind_int(stmt, 7, comment[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Supplier (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_supplier_suppkey ON SUPPLIER(SuppKey);
                CREATE INDEX idx_supplier_nationkey ON SUPPLIER(NationKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Supplier (Index): " << sqlite3_errmsg(sqlite_db))
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Supplier (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "SUPPLIER";
        return table;
    }

    EncodedTable<T> getPartSuppTable() {
        auto S = partSuppSize();

        std::vector<std::string> schema = {
            "[SuppKey]", "[PartKey]", "[SupplyCost]", "SupplyCost", "AvailQty"
        };

        // Foreign key to Part, 4 suppliers per part
        auto partkey = counterColumn(S) / PARTSUPP_PER_PART;

        // Foreign key to Supplier
        // Defined as PS_SUPPKEY = (ps_partkey + (i * (( S/4 ) + (int)(ps_partkey-1 )/S)))) modulo S + 1 
        // where i is the ith supplier within [0 .. 3] and S = SF * 10,000 i.e SupplierSize.
        Vector<T> suppkey(S);
        size_t suppSize = supplierSize();
        for (size_t row = 0, i = 0; row < S; ++row, i = ++i % 4) {
            suppkey[row] = (partkey[row] + i * ((suppSize / 4) + (partkey[row] - 1) / suppSize)) % suppSize + 1;
        }

        // 1..1000
        auto supplyCost = randomColumn(S, 1, 1000 + 1);
        
        // 1..9999
        auto availQty = randomColumn(S, 1, 9999 + 1);

        EncodedTable<T> table = secret_share<T>(
            {suppkey, partkey, supplyCost, supplyCost, availQty},
            schema
        );

        if (sqlite_db) {
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE PARTSUPP (
                    SuppKey INTEGER, PartKey INTEGER, SupplyCost INTEGER, AvailQty INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on PartSupp (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO PARTSUPP (
                    SuppKey, PartKey, SupplyCost, AvailQty)
                VALUES (?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < partSuppSize(); ++i) {
                sqlite3_bind_int(stmt, 1, suppkey[i]);
                sqlite3_bind_int(stmt, 2, partkey[i]);
                sqlite3_bind_int(stmt, 3, supplyCost[i]);
                sqlite3_bind_int(stmt, 4, availQty[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on PartSupp (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_partsupp_partkey ON PARTSUPP(PartKey);
                CREATE INDEX idx_partsupp_suppkey ON PARTSUPP(SuppKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on PartSupp (Index): " << sqlite3_errmsg(sqlite_db))
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on PartSupp (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "PARTSUPP";
        return table;
    }
    
    /**
     * @brief Get the Lineitem Table object with the SecretFlow SCQL data distribution
     * 
     * @return EncodedTable<T> 
     */
    EncodedTable<T> getLineitemTableSecretFlow(int num_rows) {
        // use fixed number of input rows per SecretFlow specification

        auto pid = runTime->getPartyID();

        size_t S = num_rows;

        // orderKey and partKey are the [S] vector
        Vector<T> orderKey = counterColumn(S);
        Vector<T> partKey = counterColumn(S);

        auto quantity = randomColumn(S, 1, 51);

        // represents a discount between 4% and 6%
        auto discount = randomColumn(S, 4, 6);
        // same thing here
        auto tax = randomColumn(S, 0, 8);

        // check this
        Vector<T> extendedPrice(S);
        for (int i = 0; i < S; i++) {
            // compute retailPrice as an intermediate step in computing extendedPrice
            T retailPrice = 90000 + ((partKey[i] / 10) % 20001) + 100 * (partKey[i] % 1000);
            extendedPrice[i] = retailPrice * quantity[i];
        }
        
        auto randomDay = randomColumn(S, 0, 365);
        auto shipDate = randomDay;
        auto commitDate = shipDate + 1;
        auto receiptDate = commitDate + 1;

        auto returnFlag = randomColumn(S, 1, 3); // randomly sample "R" or "A"
        for (int i = 0; i < S; i++) {
            if (receiptDate[i] > 168) {
                returnFlag[i] = 0; // "N"
            }
        }

        Vector<T> lineStatus(S);
        for (int i = 0; i < S; i++) {
            if (shipDate[i] > 168) {
                lineStatus[i] = 0; // "F"
            } else {
                lineStatus[i] = 1; // "O"
            }
        }

        auto shipMode = randomColumn(S, 0, 3);

        if (sqlite_db) {
            // Create table in SQLite
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE LINEITEM (
                    OrderKey INTEGER, ReturnFlag INTEGER, LineStatus INTEGER,
                    Quantity INTEGER, ExtendedPrice INTEGER, Discount INTEGER,
                    Tax INTEGER, ShipDate INTEGER, CommitDate INTEGER,
                    ReceiptDate Integer, ShipMode INTEGER, PartKey Integer
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on LineItem (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions in SQLite
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO LINEITEM (
                    OrderKey, ReturnFlag, LineStatus, Quantity, ExtendedPrice,
                    Discount, Tax, ShipDate, CommitDate, ReceiptDate, ShipMode,
                    PartKey)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, orderKey[i]);
                sqlite3_bind_int(stmt, 2, returnFlag[i]);
                sqlite3_bind_int(stmt, 3, lineStatus[i]);
                sqlite3_bind_int(stmt, 4, quantity[i]);
                sqlite3_bind_int(stmt, 5, extendedPrice[i]);
                sqlite3_bind_int(stmt, 6, discount[i]);
                sqlite3_bind_int(stmt, 7, tax[i]);
                sqlite3_bind_int(stmt, 8, shipDate[i]);
                sqlite3_bind_int(stmt, 9, commitDate[i]);
                sqlite3_bind_int(stmt, 10, receiptDate[i]);
                sqlite3_bind_int(stmt, 11, shipMode[i]);
                sqlite3_bind_int(stmt, 12, partKey[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on LineItem (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_lineitem_partkey ON LINEITEM(PartKey);
                CREATE INDEX idx_lineitem_orderkey ON LINEITEM(OrderKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on LineItem (Index): " << sqlite3_errmsg(sqlite_db))
            }

            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on LineItem (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped");
        }

        std::vector<std::string> schema = {
            "[OrderKey]",    "[ReturnFlag]",  "[LineStatus]", "Quantity",  "[Quantity]",
            "ExtendedPrice", "Discount",      "[Discount]",   "Tax",       "[ShipDate]",
            "[CommitDate]",  "[ReceiptDate]", "[ShipMode]",   "[PartKey]"};

        std::vector<Vector<T>> data = {
            orderKey, returnFlag, lineStatus,   quantity,   quantity,   extendedPrice,
            discount, discount,   tax,          shipDate,   commitDate, receiptDate,
            shipMode, partKey};

        EncodedTable<T> table = secret_share<T>(data, schema);

        table.tableName = "LINEITEMS";
        return table;
    }
    

    EncodedTable<T> getPartTableSecretFlow(int num_rows) {
        auto S = num_rows;

        std::vector<std::string> schema = {
            "[PartKey]", "[Type]"
        };

        // partKey is the [S] vector
        Vector<T> partKey(S);
        for (int i = 0; i < S; i++) {
            partKey[i]  = i;
        }

        // 1..6
        // they generate a three-part string here with 150 possibilities
        // however, they filter only on part one of the string, which has 6 possibilities
        auto type = randomColumn(S, 1, 7);


        EncodedTable<T> table = secret_share<T>({partKey, type},
                                                 schema);

        if (sqlite_db) {
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            // Create table
            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE PART (
                    PartKey INTEGER, Type INTEGER
                );
            )sql";
            char* errMsg = nullptr;
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, &errMsg);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Part (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO PART (PartKey, Type)
                VALUES (?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, partKey[i]);
                sqlite3_bind_int(stmt, 1, type[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Part (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }
            sqlite3_finalize(stmt);

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_part_partkey ON PART(PartKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Part (Index): " << sqlite3_errmsg(sqlite_db))
            }

            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Part (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "PART";
        return table;
    }


    EncodedTable<T> getOrdersTableSecretFlow(int num_rows)
    {
        auto S = num_rows;

        std::vector<std::string> schema = {
            "[OrderKey]", "[OrderPriority]"
        };

        // primary key
        Vector<T> orderKey(S);
        for (int i = 0; i < S; i++) {
            orderKey[i] = i;
        }

        // 1..4
        auto orderPriority = randomColumn(S, 1, 5);

        EncodedTable<T> table = secret_share<T>({
            orderKey, orderPriority},
            schema
        );

        if (sqlite_db) {
            // Create table
            sqlite3_exec(sqlite_db, "BEGIN TRANSACTION;", 0, 0, NULL);

            int ret;
            const char* sqlCreate = R"sql(
                CREATE TABLE ORDERS (
                    OrderKey INTEGER, OrderPriority INTEGER
                );
            )sql";
            ret = sqlite3_exec(sqlite_db, sqlCreate, 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Orders (Create): " << sqlite3_errmsg(sqlite_db))
            }

            // Prepare statement for insertions
            sqlite3_stmt* stmt = nullptr;
            const char* sqlInsert = R"sql(
                INSERT INTO ORDERS (OrderKey, OrderPriority)
                VALUES (?, ?);
            )sql";
            sqlite3_prepare_v2(sqlite_db, sqlInsert, -1, &stmt, nullptr);

            // Insert data into SQLite
            for (size_t i = 0; i < S; ++i) {
                sqlite3_bind_int(stmt, 1, orderKey[i]);
                sqlite3_bind_int(stmt, 2, orderPriority[i]);

                ret = sqlite3_step(stmt);
                if (ret != SQLITE_DONE) {
                    single_cout("SQL error on Orders (Insert): " << sqlite3_errmsg(sqlite_db))
                }
                sqlite3_reset(stmt);
            }

            // Indexes for PK/FK columns
            const char * create_indexes = R"sql(
                CREATE INDEX idx_orders_orderkey ON ORDERS(OrderKey);
            )sql";
            ret = sqlite3_exec(sqlite_db, create_indexes, 0, 0, 0);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Orders (Index): " << sqlite3_errmsg(sqlite_db))
            }

            sqlite3_finalize(stmt);
            ret = sqlite3_exec(sqlite_db, "COMMIT;", 0, 0, NULL);
            if (ret != SQLITE_OK) {
                single_cout("SQL error on Orders (Commit): " << sqlite3_errmsg(sqlite_db))
            }
        }
        else {
            single_cout("SQLite setup skipped")
        }

        table.tableName = "ORDERS";

        return table;
    }

};
