/**
 * @file ex5_join.cpp
 * @brief Example 5: Joins.
 *
 * This example shows how all types of joins work.
 *
 * We'll define two secret-shared tables:
 *
 * ATTENDEES: a database of students who went to a conference.
 *  - StudentID (unique)
 *  - GaveTalk (boolean)
 *
 * CLASSES: a database of course schedules
 *  - CourseID (non-unique; multiple courses)
 *  - StudentID (non-unique; students can take multiple courses)
 *
 * Change the sizes of the two tables (and bounds on the IDs) to see how
 * the results change.
 *
 * Different kinds of joins tell us different information about the overlap
 * between two tables:
 *
 * ```
 *              | Went to Conference | Did not go to Conference
 * -------------|--------------------|-------------------------
 *     In Class |         A          |            C
 * Not in Class |         B          |            D
 * ```
 *
 * The (plaintext) cardinalities of various joins (Attendees Join Classes)
 * are:
 * - Inner join: A
 * - Left outer join (Attendees): A + B
 * - Right outer join (Classes): A + C
 * - Full outer join: A + B + C
 *
 * Note that students in quantity `D` are not represented in either of our
 * tables.
 */

#include "orq.h"

// Tell ORQ to use the selected protocol & communicator
using namespace COMPILED_MPC_PROTOCOL_NAMESPACE;

int main(int argc, char **argv) {
    orq_init(argc, argv);
    auto pID = runTime->getPartyID();

    // Size of the two tables
    const size_t ATTENDEES_SIZE = 1000;
    const size_t CLASSES_SIZE = 1000;

    std::vector<std::string> attendee_schema = {"[StudentID]", "[GaveTalk]"};
    Vector<int> a_sid(ATTENDEES_SIZE);
    Vector<int> a_gave_talk(ATTENDEES_SIZE);

    std::vector<std::string> classes_schema = {"[StudentID]", "[CourseID]"};
    Vector<int> c_sid(CLASSES_SIZE);
    Vector<int> c_cid(CLASSES_SIZE);

    const size_t MAX_STUDENT_ID = 1000;
    const size_t MAX_CLASS_ID = 1000;

    // For example purposes, we just have P0 generate all data randomly
    if (pID == 0) {
        runTime->populateLocalRandom(a_sid);
        a_sid %= ATTENDEES_SIZE;
        runTime->populateLocalRandom(a_gave_talk);
        // just keep a single bit
        a_gave_talk.mask(1);

        runTime->populateLocalRandom(c_sid);
        c_sid %= MAX_STUDENT_ID;
        runTime->populateLocalRandom(c_cid);
        c_cid %= MAX_CLASS_ID;
    }

    using A = ASharedVector<int>;
    using B = BSharedVector<int>;

    EncodedTable<int> Attendees = secret_share<int>({a_sid, a_gave_talk}, attendee_schema);
    EncodedTable<int> Classes = secret_share<int>({c_sid, c_cid}, classes_schema);

    // Extra setup: students can't be enrolled in the same class more than once.
    // We could have handled this in plaintext, but easy to just do it under MPC
    // by calling `distinct` on the compound key. However, we could also imagine
    // that we are aggregating multiple data sources owned by different entities,
    // and do actually need to enforce a uniqueness constraint under MPC
    //
    // If we don't enforce this uniqueness constraint, joins which expect unique
    // keys will give incorrect results.
    Classes.distinct({"[CourseID]", "[StudentID]"});

    ////////////////////////////////
    // Inner Join
    //
    // `Attendees inner-join Classes` computes an intersection. We can answer
    // questions like, "Which classes had the most students give a talk at the
    // conference?"
    //
    // We put the subquery in brackets just to scope intermediate tables.
    {
        // Filter out attendees who gave a talk
        Attendees.filter(Attendees["[GaveTalk]"] == 1);

        // Join on classes
        auto Joined = Attendees.inner_join(Classes, {"[StudentID]"});
        Joined.addColumn("Count");

        auto StudentCount = Joined.deepcopy();
        StudentCount.sort({ENC_TABLE_VALID, "[StudentID]"});
        StudentCount.distinct({"[StudentID]"});
        // Count the students
        StudentCount.aggregate({ENC_TABLE_VALID}, {{"Count", "Count", count<A>}});
        // Only need that column
        StudentCount.project({"Count"});
        auto s_count = StudentCount.open_with_schema().first[0][0];
        single_cout("[Filtered Inner Join] "
                    << s_count
                    << " students went to conference AND are in classes AND gave a talk");

        // Count the students per class who gave a talk
        Joined.aggregate({"[CourseID]"}, {{"Count", "Count", count<A>}});

        Joined.deleteColumns({"[StudentID]"});

        // We can't sort on AShared columns, so need to convert.
        Joined.addColumn("[CountGaveTalk]");
        Joined.convert_a2b("Count", "[CountGaveTalk]");
        Joined.deleteColumns({"Count"});

        // Get the top few classes - sort descending and take head
        Joined.sort({ENC_TABLE_VALID, "[CountGaveTalk]"}, DESC);
        Joined.head(5);

        single_cout("Top 5 classes:");
        print_table(Joined.open_with_schema(), pID);
    }

    // Reset the tables for the next query
    // (Don't do this in real queries - better to use `deepcopy()` instead.)
    Attendees.configureValid();
    Classes.configureValid();

    ////////////////////////////////
    // Left Outer Join
    //
    // `Attendees left-outer-join Classes` includes all attendees in the output,
    // even those who aren't registered for classes.
    {
        auto Joined = Attendees.left_outer_join(Classes, {"[StudentID]"});

        Joined.sort({ENC_TABLE_VALID, "[StudentID]"});
        Joined.distinct({"[StudentID]"});

        auto opened = Joined.open_with_schema().first;
        single_cout("[LOuter Join] " << opened[0].size() << " students went to conference");
    }

    Attendees.configureValid();
    Classes.configureValid();

    ////////////////////////////////
    // Right Outer Join
    //
    // `Attendees right-outer-join Classes` includes all students in the output,
    // even those who didn't go to the conference.
    {
        auto Joined = Attendees.right_outer_join(Classes, {"[StudentID]"});

        // distinct requires a sort first
        Joined.sort({ENC_TABLE_VALID, "[StudentID]"});
        Joined.distinct({"[StudentID]"});

        auto opened = Joined.open_with_schema().first;
        single_cout("[ROuter Join] " << opened[0].size() << " students are in classes");
    }

    Attendees.configureValid();
    Classes.configureValid();

    ////////////////////////////////
    // Full Outer Join
    //
    // `Attendees full-outer-join Classes` includes all rows in the output.
    {
        auto Joined = Attendees.full_outer_join(Classes, {"[StudentID]"});

        Joined.sort({ENC_TABLE_VALID, "[StudentID]"});
        Joined.distinct({"[StudentID]"});

        auto opened = Joined.open_with_schema().first;
        single_cout("[FOuter Join] " << opened[0].size() << " total students");
    }

    Attendees.configureValid();
    Classes.configureValid();

    ////////////////////////////////
    // Anti-join
    //
    // `Attendees anti-join Classes` returns all Attendees who are not
    // registered.
    //
    // `Classes anti-join Attendees` returns all Students who are taking classes,
    // but not present in the Attendees table.
    {
        auto Joined = Classes.anti_join(Attendees, {"[StudentID]"});

        Joined.sort({ENC_TABLE_VALID, "[StudentID]"});
        Joined.distinct({"[StudentID]"});

        auto opened = Joined.open_with_schema().first;
        single_cout("[Anti Join]   " << opened[0].size()
                                     << " students in classes did not attend conference");
    }

    Attendees.configureValid();
    Classes.configureValid();

    ////////////////////////////////
    // Semi-join
    //
    // Similar to an inner-join, but only returns rows on the left. Implements
    // `EXIST` semantics.
    //
    // `Classes semi-join Attendees` returns classes which had a student attend
    // the conference.
    {
        auto Joined = Classes.semi_join(Attendees, {"[StudentID]"});

        Joined.sort({ENC_TABLE_VALID, "[CourseID]"});
        Joined.distinct({"[CourseID]"});

        Classes.sort({ENC_TABLE_VALID, "[CourseID]"});
        Classes.distinct({"[CourseID]"});
        auto c_count = Classes.open_with_schema().first[0].size();

        auto opened = Joined.open_with_schema().first;
        single_cout("[Semi Join]   " << opened[0].size() << " classes (out of " << c_count
                                     << ") had at least one student attended the conference");
    }
}