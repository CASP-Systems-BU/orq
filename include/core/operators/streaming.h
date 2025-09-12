#pragma once

#include "aggregation.h"
#include "common.h"
#include "core/containers/a_shared_vector.h"
#include "core/containers/b_shared_vector.h"

namespace orq::operators {

/**
 * @brief Computes tumbling window identifiers for stream processing.
 *
 * A tumbling window divides the stream into non-overlapping windows of fixed size.
 * This function computes the window identifier for each element by dividing the
 * key (typically a timestamp or sequence number) by the window size.
 *
 * @tparam Share The underlying data type of the shared vectors.
 * @tparam EVector Share container type.
 *
 * @param key Input vector containing keys/timestamps for window assignment.
 * @param window_size The size of each tumbling window.
 * @param res Output vector that will contain the computed window identifiers.
 */
template <typename Share, typename EVector>
static void tumbling_window(ASharedVector<Share, EVector>& key, const Share& window_size,
                            ASharedVector<Share, EVector>& res) {
    res = key / window_size;
}

/**
 * @brief Marks the start of new gap-based sessions in a timestamp sequence.
 *
 * This function identifies session boundaries based on gaps in timestamps.
 * A new session starts when the gap between consecutive timestamps exceeds
 * the specified threshold. The first element is always marked as a session start.
 *
 * @tparam Share The underlying data type of the shared vectors.
 * @tparam EVector Share container type.
 *
 * @param timestamp Input vector of timestamps in ascending order.
 * @param session_start Output boolean vector marking session start positions.
 * @param gap The maximum allowed gap between timestamps within a session.
 */
template <typename Share, typename EVector>
void mark_gap_session(ASharedVector<Share, EVector>& timestamp,
                      BSharedVector<Share, EVector>& session_start, const Share& gap) {
    // Set the first index to 1
    Vector<Share> one({1});
    session_start.slice(0, 1) =
        orq::service::runTime->public_share<EVector::replicationNumber>(one);
    ;

    Vector<Share> gap_vec(1, gap);
    ASharedVector<Share, EVector> shared_gap_vec =
        orq::service::runTime->secret_share_a<EVector::replicationNumber>(gap_vec, 0);
    auto shared_gap_vec_extended = shared_gap_vec.repeated_subset_reference(timestamp.size() - 1);

    ASharedVector<Share, EVector> pair_wise_gap =
        timestamp.slice(0, timestamp.size() - 1) + shared_gap_vec_extended - timestamp.slice(1);

    auto pair_wise_gap_b = pair_wise_gap.a2b();
    session_start.slice(1) = pair_wise_gap_b->ltz();
}

/**
 * @brief Creates gap-based session windows for stream aggregation.
 *
 * This function implements session windowing based on timestamp gaps. Sessions
 * are identified using the mark_gap_session function, then window identifiers
 * are computed using reverse aggregation to assign the same window ID to all
 * elements within a session.
 *
 * @tparam Share The underlying data type of the shared vectors.
 * @tparam EVector Share container type.
 *
 * @param keys Vector of key vectors used for aggregation operations.
 * @param timestamp_a Arithmetic shared vector of timestamps.
 * @param timestamp_b Boolean shared vector of timestamps (for multiplexing).
 * @param window_id Output vector that will contain the computed window identifiers.
 * @param gap The maximum allowed gap between timestamps within a session.
 */
template <typename Share, typename EVector>
void gap_session_window(std::vector<BSharedVector<Share, EVector>>& keys,
                        ASharedVector<Share, EVector>& timestamp_a,
                        BSharedVector<Share, EVector>& timestamp_b,
                        BSharedVector<Share, EVector>& window_id, const Share& gap) {
    mark_gap_session(timestamp_a, window_id, gap);

    Vector<Share> neg_one({-1});
    BSharedVector<Share, EVector> shared__one =
        orq::service::runTime->secret_share_b<EVector::replicationNumber>(neg_one, 0);

    window_id =
        multiplex(window_id, shared__one.repeated_subset_reference(window_id.size()), timestamp_b);

    orq::aggregators::aggregate(keys, {{window_id, window_id, orq::aggregators::max}}, {},
                                orq::aggregators::Direction::Reverse);
}

/**
 * @brief Marks the start of new threshold-based sessions.
 *
 * This function identifies session boundaries based on a threshold applied to
 * a function result (e.g., sensor readings, activity levels). A new session
 * starts when the function result exceeds the threshold and the previous
 * element was below the threshold.
 *
 * @tparam Share The underlying data type of the shared vectors.
 * @tparam EVector Share container type.
 *
 * @param function_res Input vector containing function results to compare against threshold.
 * @param session_start Output boolean vector marking session start positions.
 * @param potential_window Output boolean vector indicating elements above threshold.
 * @param threshold The threshold value for session detection.
 */
template <typename Share, typename EVector>
void mark_threshold_session(BSharedVector<Share, EVector>& function_res,
                            BSharedVector<Share, EVector>& session_start,
                            BSharedVector<Share, EVector>& potential_window,
                            const Share& threshold) {
    Vector<Share> threshold_vec({threshold});
    BSharedVector<Share, EVector> shared_threshold_vec =
        orq::service::runTime->secret_share_b<EVector::replicationNumber>(threshold_vec, 0);

    potential_window =
        function_res > shared_threshold_vec.repeated_subset_reference(function_res.size());

    session_start.slice(1) =
        potential_window.slice(1) &
        (potential_window.slice(1) ^ potential_window.slice(0, potential_window.size() - 1));
}

/**
 * @brief Creates threshold-based session windows for stream aggregation.
 *
 * This function implements session windowing based on threshold detection.
 * Sessions are identified when function results exceed a threshold, and
 * window identifiers are computed using aggregation with the potential_window
 * as a selection mask to only include elements that meet the threshold criteria.
 *
 * @tparam Share The underlying data type of the shared vectors.
 * @tparam EVector Share container type.
 *
 * @param keys Vector of key vectors used for aggregation operations.
 * @param function_res Input vector containing function results for threshold comparison.
 * @param timestamp_b Boolean shared vector of timestamps (for multiplexing).
 * @param window_id Output vector that will contain the computed window identifiers.
 * @param gap The threshold value for session detection (note: parameter name is misleading).
 */
template <typename Share, typename EVector>
void threshold_session_window(std::vector<BSharedVector<Share, EVector>>& keys,
                              BSharedVector<Share, EVector>& function_res,
                              BSharedVector<Share, EVector>& timestamp_b,
                              BSharedVector<Share, EVector>& window_id, const Share& gap) {
    BSharedVector<Share, EVector> potential_window(window_id.size());

    mark_threshold_session(function_res, window_id, potential_window, gap);

    Vector<Share> neg_one({-1});
    BSharedVector<Share, EVector> shared__one =
        orq::service::runTime->public_share<EVector::replicationNumber>(neg_one);
    BSharedVector<Share, EVector> extended_shared__one =
        shared__one.repeated_subset_reference(window_id.size());

    window_id = multiplex(window_id, extended_shared__one, timestamp_b);

    orq::aggregators::aggregate(keys, {{window_id, window_id, orq::aggregators::max}}, {},
                                potential_window);
}
}  // namespace orq::operators
