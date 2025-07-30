namespace secrecy
{
    namespace aggregators
    {
        template <typename A, typename B>
        class AggregationSelector
        {
            using af_t = void (*)(const A &, A &, const A &);
            using bf_t = void (*)(const B &, B &, const B &);

        private:
            bf_t bFunc;
            af_t aFunc;

        public:
            AggregationSelector(bf_t bFunc) : bFunc(bFunc), aFunc(nullptr)
            {
            }

            AggregationSelector(af_t aFunc) : aFunc(aFunc), bFunc(nullptr)
            {
            }

            af_t getA()
            {
                if (aFunc == nullptr)
                {
                    std::cerr << "Error: requested A func, but was null\n";
                    exit(-1);
                }
                return aFunc;
            }

            bf_t getB()
            {
                if (bFunc == nullptr)
                {
                    std::cerr << "Error: requested B func, but was null\n";
                    exit(-1);
                }

                return bFunc;
            }

            bool isAggregation()
            {
                return !(aFunc == &copy<A> || bFunc == &copy<B> || bFunc == &valid<B>);
            }
        };
    }
}