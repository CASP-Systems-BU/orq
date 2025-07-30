#ifndef GAVEL_BENCHMARKING_MEMORY_H
#define GAVEL_BENCHMARKING_MEMORY_H

static void test_same_array(int size) {
    long long *arr_1 = new long long[size];
    long long *arr_2 = new long long[size];

    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_1[i] = random();
#else
        arr_1[i] = 5;
#endif
    }

    for (int j = 0; j < 10; ++j) {
        for (int i = 0; i < size; ++i) {
#ifdef random_generation
            arr_2[i] *= (arr_1[i] * random());
#else
//            arr_2[i] *= (arr_1[i] * j);
            arr_2[i] *= (j);
#endif
            if (j == 9 && i == size - 1) {
                printf("%lld\t", arr_2[i]);
            }
        }
    }
    free(arr_1);
    free(arr_2);
}


void test_different_array(int size) {
    long long *arr_1 = new long long[size];
//    long long *arr_2 = new long long[size];
//    long long *arr_3 = new long long[size];
//    long long *arr_4 = new long long[size];
//    long long *arr_5 = new long long[size];
//    long long *arr_6 = new long long[size];
//    long long *arr_7 = new long long[size];
//    long long *arr_8 = new long long[size];
//    long long *arr_9 = new long long[size];
//    long long *arr_10 = new long long[size];
//    long long *arr_11 = new long long[size];

//long long *arr_2, *arr_3, *arr_4, *arr_5, *arr_6, *arr_7, *arr_8, *arr_9, *arr_10, *arr_11;
//arr_2 = arr_3 = arr_4 = arr_5 = arr_6 = arr_7 = arr_8 = arr_9 = arr_10 = arr_11 = arr_1;


    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_1[i] = random();
#else
        arr_1[i] = 5;
#endif
    }

    long long *arr_2 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_2[i] = arr_1[i] * random();
#else
        arr_2[i] = arr_1[i] * 5;
#endif
    }
    free(arr_1);

    long long *arr_3 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_3[i] = arr_2[i] * random();
#else
        arr_3[i] = arr_2[i] * 5;
#endif
    }
    free(arr_2);

    long long *arr_4 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_4[i] = random() * arr_3[i];
#else
        arr_4[i] = 5 * arr_3[i];
#endif
    }
    free(arr_3);

    long long *arr_5 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_5[i] = random() * arr_4[i];
#else
        arr_5[i] = 5 * arr_4[i];
#endif
    }
    free(arr_4);

    long long *arr_6 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_6[i] = random() * arr_5[i];
#else
        arr_6[i] = 5 * arr_5[i];
#endif
    }
    free(arr_5);

    long long *arr_7 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_7[i] = random() * arr_6[i];
#else
        arr_7[i] = 5 * arr_6[i];
#endif
    }
    free(arr_6);

    long long *arr_8 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_8[i] = random() * arr_7[i];
#else
        arr_8[i] = 5 * arr_7[i];
#endif
    }
    free(arr_7);

    long long *arr_9 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_9[i] = random() * arr_8[i];
#else
        arr_9[i] = 5 * arr_8[i];
#endif
    }
    free(arr_8);

    long long *arr_10 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_10[i] = random() * arr_9[i];
#else
        arr_10[i] = 5 * arr_9[i];
#endif
    }
    free(arr_9);

    long long *arr_11 = new long long[size];
    for (int i = 0; i < size; ++i) {
#ifdef random_generation
        arr_11[i] = random() * arr_10[i];
#else
        arr_11[i] = 5 * arr_10[i];
#endif
        if (i == size - 1) {
            printf("%lld \t", arr_11[i]);
        }
    }
    free(arr_10);
    free(arr_11);
}


#endif //GAVEL_BENCHMARKING_MEMORY_H
