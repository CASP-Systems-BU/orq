#include "../include/planner/database.h"


Database::Database(const std::string &name, 
                    const std::vector<Relation> &relations, 
                    const std::string &version) :
        name(name), relations(relations), versionName(version) {
            // Create relation index
            for (auto r : this->relations) {
                auto ret = this->index.insert(std::pair<std::string, Relation> (r.getName(), r));
                if (ret.second == false) {
                    std::cout << "[ERROR]: Duplicate relation found." << std::endl;
                    exit(-1);
                }
            }
}

Database::~Database() {}

void Database::addFKs(std::map<std::string, std::string> &fks) {
    this->fks = fks;
}
