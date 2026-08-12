#include <stdint.h>

#include "powertrain_compound_calibration.h"
#include "target_diagnostics.h"

#define STAR_POINT_COUNT       (9U)
#define STAR_SAMPLE_COUNT      (40U)
#define STAR_WARMUP_COUNT      (8U)
#define STAR_CORE_MHZ          (48U)
#define STAR_DEMCR             (*(volatile uint32_t *)0xE000EDFCUL)
#define STAR_DWT_CTRL          (*(volatile uint32_t *)0xE0001000UL)
#define STAR_DWT_CYCCNT        (*(volatile uint32_t *)0xE0001004UL)
#define STAR_TRCENA_MASK       (1UL << 24U)
#define STAR_CYCCNT_MASK       (1UL << 0U)

typedef void (*StarFunction)(void);
typedef struct { uint32_t feature_group; uint32_t work_units; StarFunction function; } StarDescriptor;
typedef struct { uint32_t feature_group; uint32_t work_units; uint32_t completed_samples; uint32_t mean_execution_us_x1000; uint32_t max_execution_us_x1000; } StarPoint;
typedef struct { uint32_t point_count; uint32_t sample_count; uint32_t measurement_overhead_cycles; uint32_t error_flags; StarPoint points[STAR_POINT_COUNT]; } StarResults;

static const StarDescriptor g_star_points[STAR_POINT_COUNT] =
{
    { 0U, 8U, StarCalibration_TorqueMap_8 },
    { 0U, 16U, StarCalibration_TorqueMap_16 },
    { 0U, 32U, StarCalibration_TorqueMap_32 },
    { 1U, 4U, StarCalibration_Diagnostics_4 },
    { 1U, 8U, StarCalibration_Diagnostics_8 },
    { 1U, 16U, StarCalibration_Diagnostics_16 },
    { 2U, 4U, StarCalibration_CanPacking_4 },
    { 2U, 8U, StarCalibration_CanPacking_8 },
    { 2U, 16U, StarCalibration_CanPacking_16 }
};

volatile uint32_t g_star_compound_calibration_complete;
volatile uint32_t g_star_compound_calibration_current;
volatile StarResults g_star_compound_calibration_results;

static uint32_t StarCyclesToUsX1000(uint64_t cycles)
{
    return (uint32_t)((cycles * UINT64_C(1000)) / STAR_CORE_MHZ);
}

static uint32_t StarMeasureOverhead(void)
{
    uint32_t index;
    uint32_t minimum = UINT32_MAX;
    for (index = 0U; index < 64U; ++index)
    {
        const uint32_t start = STAR_DWT_CYCCNT;
        const uint32_t elapsed = STAR_DWT_CYCCNT - start;
        if (elapsed < minimum) { minimum = elapsed; }
    }
    return minimum;
}

static void StarMeasurePoint(uint32_t point_index)
{
    volatile StarPoint *point = &g_star_compound_calibration_results.points[point_index];
    uint64_t total = 0U;
    uint32_t maximum = 0U;
    uint32_t sample;

    point->feature_group = g_star_points[point_index].feature_group;
    point->work_units = g_star_points[point_index].work_units;
    g_star_compound_calibration_current = point_index;
    for (sample = 0U; sample < STAR_WARMUP_COUNT; ++sample)
    {
        StarCompoundCalibration_Initialize();
        g_star_points[point_index].function();
    }
    for (sample = 0U; sample < STAR_SAMPLE_COUNT; ++sample)
    {
        uint32_t start;
        uint32_t elapsed;
        StarCompoundCalibration_Initialize();
        start = STAR_DWT_CYCCNT;
        g_star_points[point_index].function();
        elapsed = STAR_DWT_CYCCNT - start;
        if (elapsed >= g_star_compound_calibration_results.measurement_overhead_cycles)
        {
            elapsed -= g_star_compound_calibration_results.measurement_overhead_cycles;
        }
        else
        {
            g_star_compound_calibration_results.error_flags |= 2U;
        }
        total += elapsed;
        if (elapsed > maximum) { maximum = elapsed; }
    }
    point->completed_samples = STAR_SAMPLE_COUNT;
    point->mean_execution_us_x1000 = StarCyclesToUsX1000(total / STAR_SAMPLE_COUNT);
    point->max_execution_us_x1000 = StarCyclesToUsX1000(maximum);
}

int main(void)
{
    uint32_t point_index;
    rts_s32k148_timing_initialize();
    STAR_DEMCR |= STAR_TRCENA_MASK;
    STAR_DWT_CYCCNT = 0U;
    STAR_DWT_CTRL |= STAR_CYCCNT_MASK;
    g_star_compound_calibration_results.point_count = STAR_POINT_COUNT;
    g_star_compound_calibration_results.sample_count = STAR_SAMPLE_COUNT;
    g_star_compound_calibration_results.measurement_overhead_cycles = StarMeasureOverhead();
    for (point_index = 0U; point_index < STAR_POINT_COUNT; ++point_index)
    {
        StarMeasurePoint(point_index);
    }
    g_star_compound_calibration_complete =
        (g_star_compound_calibration_results.error_flags == 0U) ? 1U : 2U;
    for (;;) { __asm__ volatile ("nop"); }
}
