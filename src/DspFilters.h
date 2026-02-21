#ifndef DSP_FILTERS_H
#define DSP_FILTERS_H

class EmaFilter {
public:
    EmaFilter(float alpha = 0.1f);
    void setAlpha(float alpha);
    float update(float input);
    float getValue() const;
    void reset(float initialValue = 0.0f);

private:
    float _alpha;
    float _value;
};

class DcBlocker {
public:
    DcBlocker(float alpha = 0.95f); // High-pass alpha
    float process(float input);
    void reset();

private:
    float _alpha;
    float _prevInput;
    float _prevOutput;
};

#endif
