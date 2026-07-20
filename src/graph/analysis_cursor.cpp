#include "graph/analysis_cursor.hpp"

namespace graph {

const char* analysis_op_name(AnalysisOp op) {
    switch (op) {
        case AnalysisOp::kValue:
            return "Value";
        case AnalysisOp::kZero:
            return "Zero";
        case AnalysisOp::kMinimum:
            return "Minimum";
        case AnalysisOp::kMaximum:
            return "Maximum";
        case AnalysisOp::kIntersect:
            return "Intersect";
        case AnalysisOp::kDerivative:
            return "dy/dx";
        case AnalysisOp::kIntegral:
            return "Integral";
    }
    return "";
}

void AnalysisSession::begin(AnalysisOp o, int first_slot) {
    active = true;
    done = false;
    op = o;
    slot = first_slot;
    slot2 = -1;
    step = 0;
    vals[0] = vals[1] = vals[2] = 0;
    result = AnalysisResult{};
}

void AnalysisSession::cancel() {
    active = false;
    done = false;
    slot2 = -1;
    step = 0;
}

int AnalysisSession::steps_needed() const {
    switch (op) {
        case AnalysisOp::kValue:
        case AnalysisOp::kDerivative:
            return 1;
        case AnalysisOp::kIntegral:
            return 2;
        default:  // zero, min, max, intersect
            return 3;
    }
}

bool AnalysisSession::curve_pick() const {
    return op == AnalysisOp::kIntersect && step < 2;
}

bool AnalysisSession::slot_locked() const {
    if (op == AnalysisOp::kIntersect) {
        return step >= 2;
    }
    return step >= 1;
}

const char* AnalysisSession::prompt(Mode mode) const {
    switch (op) {
        case AnalysisOp::kValue:
        case AnalysisOp::kDerivative:
            switch (mode) {
                case Mode::kParametric:
                    return "T?";
                case Mode::kPolar:
                    return "th?";
                default:
                    return "X?";
            }
        case AnalysisOp::kIntegral:
            return step == 0 ? "Lower Limit?" : "Upper Limit?";
        case AnalysisOp::kIntersect:
            if (step == 0) {
                return "First curve?";
            }
            return step == 1 ? "Second curve?" : "Guess?";
        default:  // zero, min, max
            if (step == 0) {
                return "Left Bound?";
            }
            return step == 1 ? "Right Bound?" : "Guess?";
    }
}

bool AnalysisSession::commit(int cursor_slot, double indep) {
    if (!active) {
        return false;
    }
    if (curve_pick()) {
        if (step == 0) {
            slot = cursor_slot;
        } else {
            if (cursor_slot == slot) {
                return false;  // Need two distinct curves
            }
            slot2 = cursor_slot;
        }
    } else {
        if (!slot_locked()) {
            slot = cursor_slot;
        }
        vals[step] = indep;
    }
    ++step;
    return step >= steps_needed();
}

void AnalysisSession::compute(const GraphState& st) {
    switch (op) {
        case AnalysisOp::kValue:
            result = analyze_value(st, slot, vals[0]);
            break;
        case AnalysisOp::kZero:
            result = analyze_zero(st, slot, vals[0], vals[1], vals[2]);
            break;
        case AnalysisOp::kMinimum:
        case AnalysisOp::kMaximum:
            // The Guess step (vals[2]) is accepted for TI flow parity
            // but Brent only needs the bracket.
            result = analyze_extremum(st, slot, vals[0], vals[1], op == AnalysisOp::kMaximum);
            break;
        case AnalysisOp::kIntersect:
            result = analyze_intersect(st, slot, slot2, vals[2]);
            break;
        case AnalysisOp::kDerivative:
            result = analyze_derivative(st, slot, vals[0]);
            break;
        case AnalysisOp::kIntegral:
            result = analyze_integral(st, slot, vals[0], vals[1]);
            break;
    }
    active = false;
    done = true;
}

}  // namespace graph
