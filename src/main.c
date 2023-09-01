#include <err.h>

#include "render.h"
#include "display.h"
#include "timing.h"
#include "stats.h"

#define PLOT_WIDTH 38
#define PLOT_HEIGHT 10
#define UPD_PER_SEC 1.0

int main() {
	init_render("bitmaps");
	int display = init_display("/dev/ttyUSB0", 666666);
	struct Bitmap template = load_exp_pbm("bitmaps/template.pbm", 128, 64);


	struct Area area;
	alloc_area(&area, 128, 64);
	render_bitmap(&area, &template);

	struct Area cpu_plot_area;
	subarea(&area, &cpu_plot_area, 0, 0, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area cpu_tmp_plot_area;
	subarea(&area, &cpu_tmp_plot_area, 0, 12, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area ram_tmp_plot_area;
	subarea(&area, &ram_tmp_plot_area, 0, 42, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area ram_plot_area;
	subarea(&area, &ram_plot_area, 0, 54, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area net_tx_area;
	subarea(&area, &net_tx_area, 90, 0, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area net_rx_area;
	subarea(&area, &net_rx_area, 90, 12, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area disk_r_area;
	subarea(&area, &disk_r_area, 90, 42, PLOT_WIDTH, PLOT_HEIGHT);

	struct Area disk_w_area;
	subarea(&area, &disk_w_area, 90, 54, PLOT_WIDTH, PLOT_HEIGHT);


	struct Area cpu_scalar_area;
	subarea(&area, &cpu_scalar_area, 49, 6, 11, 4);

	struct Area cpu_tmp_scalar_area;
	subarea(&area, &cpu_tmp_scalar_area, 49, 12, 11, 4);

	struct Area ram_tmp_scalar_area;
	subarea(&area, &ram_tmp_scalar_area, 49, 48, 11, 4);

	struct Area ram_scalar_area;
	subarea(&area, &ram_scalar_area, 49, 54, 11, 4);

	struct Area net_tx_scalar_area;
	subarea(&area, &net_tx_scalar_area, 53, 0, 33, 4);

	struct Area net_rx_scalar_area;
	subarea(&area, &net_rx_scalar_area, 53, 18, 33, 4);

	struct Area disk_r_scalar_area;
	subarea(&area, &disk_r_scalar_area, 53, 42, 33, 4);

	struct Area disk_w_scalar_area;
	subarea(&area, &disk_w_scalar_area, 53, 60, 33, 4);


	struct Area uptime_days_area;
	subarea(&area, &uptime_days_area, 100, 25, 11, 4);

	struct Area uptime_hours_area;
	subarea(&area, &uptime_hours_area, 104, 30, 7, 4);

	struct Area uptime_minutes_area;
	subarea(&area, &uptime_minutes_area, 104, 35, 7, 4);


	struct Area fan1_area;
	subarea(&area, &fan1_area, 0, 25, 15, 4);

	struct Area fan2_area;
	subarea(&area, &fan2_area, 0, 30, 15, 4);

	struct Area fan3_area;
	subarea(&area, &fan3_area, 0, 35, 15, 4);


	struct Ring cpu_ring;
	alloc_ring(&cpu_ring, PLOT_WIDTH);

	struct Ring cpu_tmp_ring;
	alloc_ring(&cpu_tmp_ring, PLOT_WIDTH);

	struct Ring ram_tmp_ring;
	alloc_ring(&ram_tmp_ring, PLOT_WIDTH);

	struct Ring ram_ring;
	alloc_ring(&ram_ring, PLOT_WIDTH);

	struct Ring net_rx_ring;
	alloc_ring(&net_rx_ring, PLOT_WIDTH);

	struct Ring net_tx_ring;
	alloc_ring(&net_tx_ring, PLOT_WIDTH);

	struct Ring disk_r_ring;
	alloc_ring(&disk_r_ring, PLOT_WIDTH);

	struct Ring disk_w_ring;
	alloc_ring(&disk_w_ring, PLOT_WIDTH);

	
	double next_update = get_time() + 1.0 / UPD_PER_SEC;
	get_stats(); // removes first run garbage

	for (;;) {
		if (!sleep_until(next_update))
			errx(1, "failed to render frame in time");
		next_update += 1.0 / UPD_PER_SEC;

		struct Stats stats = get_stats();

		push_ring(&cpu_ring, stats.cpu);
		push_ring(&cpu_tmp_ring, stats.cpu_tmp);
		push_ring(&ram_tmp_ring, stats.ram_tmp);
		push_ring(&ram_ring, stats.ram);
		push_ring(&net_rx_ring, stats.net_rx);
		push_ring(&net_tx_ring, stats.net_tx);
		push_ring(&disk_r_ring, stats.disk_r);
		push_ring(&disk_w_ring, stats.disk_w);

		render_scalar(&cpu_scalar_area, stats.cpu * 100);
		render_scalar(&cpu_tmp_scalar_area, stats.cpu_tmp);
		render_scalar(&ram_tmp_scalar_area, stats.ram_tmp);
		render_scalar(&ram_scalar_area, stats.ram * 100);

		render_scalar_prefixed(&net_rx_scalar_area, stats.net_rx);
		render_scalar_prefixed(&net_tx_scalar_area, stats.net_tx);
		render_scalar_prefixed(&disk_r_scalar_area, stats.disk_r);
		render_scalar_prefixed(&disk_w_scalar_area, stats.disk_w);

		render_scalar(&uptime_days_area, stats.days);
		render_scalar(&uptime_hours_area, stats.hours);
		render_scalar(&uptime_minutes_area, stats.minutes);

		render_scalar(&fan1_area, stats.fan1);
		render_scalar(&fan2_area, stats.fan2);
		render_scalar(&fan3_area, stats.fan3);

		render_plot(&cpu_plot_area, &cpu_ring);
		render_plot_fluct(&cpu_tmp_plot_area, &cpu_tmp_ring);
		render_plot_fluct(&ram_tmp_plot_area, &ram_tmp_ring);
		render_plot(&ram_plot_area, &ram_ring);
		render_plot_norm(&net_rx_area, &net_rx_ring);
		render_plot_norm(&net_tx_area, &net_tx_ring);
		render_plot_norm(&disk_r_area, &disk_r_ring);
		render_plot_norm(&disk_w_area, &disk_w_ring);

		draw_display(display, &area);
	}
}
