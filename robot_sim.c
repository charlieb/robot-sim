#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>

#define LEFT 0
#define RIGHT 1
#define PEN 2

#define LEF_MASK 0b110000
#define RIG_MASK 0b001100
#define PEN_MASK 0b000011

#define LEF_SHIFT 4
#define RIG_SHIFT 2
#define PEN_SHIFT 0

#define POS_CHAR '+'
#define NEG_CHAR '-'
#define NOP_CHAR '.'

#define POS_NUM 0b01
#define NEG_NUM 0b10
#define NOP_NUM 0b00
/**
 * File format is 3 characters per line.
 * Each character is either '+', '-' or any other usually '.'.
 * + means step clockwise or put pen down
 * - means step anti-clockwise or pick pen up
 * . means no action for this time-step
 *
 * The first character is the left motor command.
 * The second character is the right motor command.
 * The third character is the pen command.
 */

/*********** PACKING AND UNPACKING TRIPLETS **************/

char to_num(char ch)
{
    switch(ch) {
        case POS_CHAR: return POS_NUM;
        case NEG_CHAR: return NEG_NUM;
        default: return NOP_NUM;
    }
}

char to_char(char ch)
{
    switch(ch) {
        case POS_NUM: return POS_CHAR;
        case NEG_NUM: return NEG_CHAR;
        default: return NOP_CHAR;
    }
}

unsigned int pack(char ins[3])
{
    unsigned int res = 0;
    res += to_num(ins[LEFT]) << LEF_SHIFT;
    res += to_num(ins[RIGHT]) << RIG_SHIFT;
    res += to_num(ins[PEN]) << PEN_SHIFT;
    return res;
}

void unpack(char ins[3], unsigned int val)
{
    ins[LEFT] = to_char((char)((val & LEF_MASK) >> LEF_SHIFT));
    ins[RIGHT] = to_char((char)((val & RIG_MASK) >> RIG_SHIFT));
    ins[PEN] = to_char((char)((val & PEN_MASK) >> PEN_SHIFT));
}

void pack_unpack_test()
{
    char test[3][3] = {"+-.", ".+-", "-.+"};
    g_assert(pack(test[0]) == 0b011000);


    char out[3];
    for(int i = 0; i < 3; i++) {
        memset(out, 0, 3);
        unpack(out, pack(test[i]));
        /* printf("%c%c%c\n", test[i][0], test[i][1], test[i][2]); */
        /* printf("%c%c%c\n", out[0], out[1], out[2]); */
        g_assert(0 == strncmp(test[i], out, 3));
    }
}

/********** INPUT FILE READING *******************/
/**
 * Reads a file of triples, one on each line.
 * If it encounters a bad line it will continue and return the
 * negative 1-indexed line number, otherwise returns the number of lines read
 * without incedent.
 * */
int read_file(FILE *f, GList **list)
{
    char line[3];
    unsigned int i = 0;
    int err = 0;
    while(!feof(f)) {
        i++;
        if(3 != fscanf(f, "%c%c%c\n", &line[0], &line[1], &line[2])) 
            err = -i;
        else 
            *list = g_list_prepend(*list, GINT_TO_POINTER(pack(line)));
        /* printf("%c%c%c\n", line[0], line[1], line[2]); */

        /* GList *first = g_list_first(*list); */
        /* if(first) { */
        /*     // printf("%p\n", first->data); */
        /*     char out[3]; */
        /*     unpack(out, GPOINTER_TO_INT(first->data)); */
        /*     printf("%c%c%c\n", out[0], out[1], out[2]); */
        /* } */
    }
    return err ? err : i;
}
// that was easier than I expected!

GList *read_data(char *fname)
{
    GList *triples = NULL;
    FILE *f = fopen(fname, "r");
    if(!f) {
        printf("Failed to open file %s\n", fname);
        return NULL;
    }
    int read_res = read_file(f, &triples);
    if(read_res < 0) 
        printf("Non-fatal error reading file, last error found on line %i\n", -read_res);
    
    fclose(f);
    triples = g_list_reverse(triples);
    printf("Read %i steps\n", g_list_length(triples));
    return triples;
}

/***************** GEOMETRY *********************/
enum robot_type { wires, planar, elbow };
struct draw_data {
    GList *steps;
    GList *poses;
    guint pos;
    float *pos_data;
    enum robot_type type;
    float paper_offset_y;
    float paper_offset_x;
    float spool_distance;
};
void to_coords(float *x, float *y, float spool_dist, float llen, float rlen, enum robot_type type)
{
    if(llen + rlen < spool_dist) {
        printf("You snapped a string!\n"); fflush(NULL); 
        exit(1);
    }
    float xf = (spool_dist*spool_dist + llen*llen - rlen*rlen) / (2.0 * spool_dist);
    *x = xf;
    *y = sqrt(llen*llen - xf*xf);
}

void recalc_draw_data(struct draw_data *data)
{
    const float ustep = 1.0 / 320.0;
    gboolean pen_down = FALSE;
    float x, y;
    float rlen = 0.6 * data->spool_distance;
    float llen = 0.6 * data->spool_distance;
    char lrp[3]; // left, right, pen
    int nposes = 0;

    g_list_free(data->poses);
    if(data->pos_data != NULL) free(data->pos_data);

    // allocate enough memory for one set of coords for every step.
    data->pos_data = malloc(2 * g_list_length(data->steps) * sizeof(float));
    memset(data->pos_data, 0, 2 * g_list_length(data->steps) * sizeof(float));

    for(GList *step = data->steps; step != NULL; step = step->next) {
        // Process the movement
        unpack(lrp, GPOINTER_TO_INT(step->data));
        //printf("%c%c%c\n", lrp[0], lrp[1], lrp[2]);
        switch(lrp[LEFT]) {
            case POS_CHAR: llen += ustep; break;
            case NEG_CHAR: llen -= ustep; break;
        }
        switch(lrp[RIGHT]) {
            case POS_CHAR: rlen += ustep; break;
            case NEG_CHAR: rlen -= ustep; break;
        }
        switch(lrp[PEN]) {
            case POS_CHAR: pen_down = TRUE; break;
            case NEG_CHAR: pen_down = FALSE; break;
        }
        
        // Was there actually a movement worth recording?
        // not if the pen wasn't down there wasn't!
        if(!pen_down) continue;
        to_coords(&x, &y, data->spool_distance, llen, rlen, data->type);
        //printf("(%f,%f)\n", x,y);
        data->poses = g_list_prepend(data->poses, &data->pos_data[nposes * 2]);
        data->pos_data[nposes * 2 + 0] = x;
        data->pos_data[nposes * 2 + 1] = y;
        nposes++;
    }
    data->poses = g_list_reverse(data->poses);
}

/******************* UI *************************/

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event, 
        gpointer gdata)
{
    struct draw_data *data = gdata;
    const float pi2 = 6.28318530718;
    const float paper_size_x = 355.6;
    const float paper_size_y = 215.9;

    int x;

    // Figure out the size of our "real-life" drawing area
    // Assume spools are placed wider than the paper width
    float maxx = data->spool_distance;
    // US legal = 8.5 x 14 inches = 215.9 x 355.6 mm (+ 1cm margin)
    float maxy = data->paper_offset_y + paper_size_y + 10.0;

    // Scale x and y to the desired aspect ratio
    // The result is that x and y define the area 
    // we want to draw in and consider the rest of 
    // the drawable as dead space
    // e.g. if the window is really tall and narrow we 
    // have to ignore most of the bottom and draw tiny in the top
    float ratio = maxx / maxy;
    if(widget->allocation.width / (float)widget->allocation.height < ratio)
      x = widget->allocation.width;
    else
      x = widget->allocation.height * ratio;
    // Now figure out the scaling factor to fit the spools
    // and paper onto our reduced drawing area
    float scale = x / maxx;
    
    cairo_t *cr = gdk_cairo_create(widget->window);
    cairo_set_source_rgb(cr,1,1,1);
    cairo_fill(cr);

    cairo_set_source_rgb(cr,1,0,0);
    cairo_rectangle(cr,
                    data->paper_offset_x * scale, 
                    data->paper_offset_y * scale, 
                    paper_size_x * scale,
                    paper_size_y * scale);
    cairo_stroke(cr);

    cairo_set_source_rgb(cr,0.5,0.5,0.5);
    cairo_arc(cr, 0, 0, 20, 0, pi2);
    cairo_fill(cr);
    cairo_arc(cr, x, 0, 20, 0, pi2);
    cairo_fill(cr);

    cairo_set_source_rgb(cr,0,0,0);
    for(GList *points = data->poses; points != NULL; points = points->next) {
      cairo_arc(cr, 
                ((float*)points->data)[0] * scale,
                ((float*)points->data)[1] * scale,
                2,
                0, pi2);
      cairo_fill(cr);
    }

    cairo_destroy(cr);
    return TRUE;
}

static gboolean keypress(GtkWidget *widget,
                             GdkEventKey *event,
                             gpointer data)
{
  switch(event->keyval) {
    case GDK_q:
      g_signal_emit_by_name(widget, "delete-event");
  }
  return FALSE;
}

static gboolean delete_event(GtkWidget *widget,
                             GdkEvent  *event,
                             gpointer   data )
{
    g_print ("delete event occurred\n");

    // Allow window to be destroyed
    return FALSE;
}

static void destroy(GtkWidget *widget, gpointer data)
{
    gtk_main_quit ();
}

static void start_pressed(GtkWidget *widget, gpointer sliderp)
{
  gtk_range_set_value(sliderp, 0);
}

static void unstep_pressed(GtkWidget *widget, gpointer sliderp)
{
  gtk_range_set_value(sliderp, gtk_range_get_value(sliderp) - 1);
}

static void step_pressed(GtkWidget *widget, gpointer sliderp)
{
  gtk_range_set_value(sliderp, gtk_range_get_value(sliderp) + 1);
}

static void end_pressed(GtkWidget *widget, gpointer sliderp)
{
  gtk_range_set_value(sliderp, 
      gtk_adjustment_get_upper(gtk_range_get_adjustment(sliderp)));
}
static void paper_offset_x_changed(GtkSpinButton *widget, gpointer gdata, gpointer unused)
{
  ((struct draw_data*)gdata)->paper_offset_x = gtk_spin_button_get_value(widget);
  GdkWindow *window = gtk_widget_get_window(gtk_widget_get_toplevel(GTK_WIDGET(widget)));
  gdk_window_invalidate_rect(window, NULL, TRUE);
}
static void paper_offset_y_changed(GtkSpinButton *widget, gpointer gdata, gpointer unused)
{
  ((struct draw_data*)gdata)->paper_offset_y = gtk_spin_button_get_value(widget);
  GdkWindow *window = gtk_widget_get_window(gtk_widget_get_toplevel(GTK_WIDGET(widget)));
  gdk_window_invalidate_rect(window, NULL, TRUE);
}
static void spool_dist_changed(GtkSpinButton *widget, gpointer gdata, gpointer unused)
{
  ((struct draw_data*)gdata)->spool_distance = gtk_spin_button_get_value(widget);
  GdkWindow *window = gtk_widget_get_window(gtk_widget_get_toplevel(GTK_WIDGET(widget)));
  gdk_window_invalidate_rect(window, NULL, TRUE);
}
static gboolean slider_moved(GtkRange *range, 
                             GtkScrollType scroll,
                             gdouble value,
                             gpointer data)
{
  gtk_range_set_value(range, 
      gtk_adjustment_get_upper(gtk_range_get_adjustment(range)));
  return TRUE;
}

GtkWidget *control_bar(struct draw_data *data)
{
  // GtkWidget *
  // gtk_hbox_new (gboolean homogeneous,
  //               gint spacing);
  GtkWidget *box = gtk_vbox_new(TRUE, 10);
  
  GtkWidget *playback_bar = gtk_hbox_new(FALSE, 10);
  GtkWidget *start = gtk_button_new_with_label("|<");
  GtkWidget *unstep = gtk_button_new_with_label("<");
  GtkWidget *step = gtk_button_new_with_label(">");
  GtkWidget *end = gtk_button_new_with_label(">|");
  GtkWidget *slider = gtk_hscale_new_with_range(0,100,1);

  GtkWidget *offset_bar = gtk_hbox_new(FALSE, 1);
  GtkWidget *paper_x_label = gtk_label_new("Paper offset (mm) X:");
  GtkWidget *paper_offset_x = gtk_spin_button_new_with_range(0,1000,1);
  GtkWidget *paper_y_label = gtk_label_new("Y:");
  GtkWidget *paper_offset_y = gtk_spin_button_new_with_range(0,1000,1);
  GtkWidget *spool_dist = gtk_spin_button_new_with_range(0,1000,1);
  GtkWidget *spool_dist_label = gtk_label_new("Distance between spools (mm):");

  gtk_spin_button_set_value(GTK_SPIN_BUTTON(paper_offset_x), data->paper_offset_x);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(paper_offset_y), data->paper_offset_y);
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(spool_dist), data->spool_distance);
  //gtk_label_set_justify(GTK_LABEL(paper_y_label), GTK_JUSTIFY_RIGHT);
  //gtk_misc_set_alignment(GTK_MISC(paper_y_label), paper_y_label->allocation.width, paper_y_label->allocation.height/2);

  /*
   * gtk_box_pack_start (GtkBox *box,
   *                     GtkWidget *child,
   *                     gboolean expand,
   *                     gboolean fill,
   *                     guint padding);
   */
  gtk_box_pack_start(GTK_BOX(playback_bar), start, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(playback_bar), unstep, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(playback_bar), step, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(playback_bar), end, TRUE, TRUE, 0);
  
  gtk_box_pack_start(GTK_BOX(offset_bar), paper_x_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(offset_bar), paper_offset_x, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(offset_bar), paper_y_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(offset_bar), paper_offset_y, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(offset_bar), spool_dist_label, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(offset_bar), spool_dist, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(box), playback_bar, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), slider, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), offset_bar, TRUE, TRUE, 0);

  g_signal_connect(start, "clicked", G_CALLBACK(start_pressed), slider);
  g_signal_connect(unstep, "clicked", G_CALLBACK(unstep_pressed), slider);
  g_signal_connect(step, "clicked", G_CALLBACK(step_pressed), slider);
  g_signal_connect(end, "clicked", G_CALLBACK(end_pressed), slider);
  g_signal_connect(slider, "change-value", G_CALLBACK(slider_moved), data);

  g_signal_connect(paper_offset_x, "value-changed", G_CALLBACK(paper_offset_x_changed), data);
  g_signal_connect(paper_offset_y, "value-changed", G_CALLBACK(paper_offset_y_changed), data);
  g_signal_connect(spool_dist, "value-changed", G_CALLBACK(spool_dist_changed), data);

  gtk_widget_show(start);
  gtk_widget_show(unstep);
  gtk_widget_show(step);
  gtk_widget_show(end);
  gtk_widget_show(playback_bar);

  gtk_widget_show(slider);

  gtk_widget_show(paper_x_label);
  gtk_widget_show(paper_offset_y);
  gtk_widget_show(paper_y_label);
  gtk_widget_show(paper_offset_x);
  gtk_widget_show(spool_dist_label);
  gtk_widget_show(spool_dist);
  gtk_widget_show(offset_bar);

  gtk_widget_show(box);

  return box;
}

void launch_ui(int argc, char **argv)
{

    gtk_init(&argc, &argv);

    struct draw_data data = {.paper_offset_y=20.0, .spool_distance = 400.0,};

    data.steps = read_data("input.txt");
    data.type = wires;
    recalc_draw_data(&data);

    // Main window
    GtkWidget *window;
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    // Main window close signal handlers
    g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);
    // Handle keyboard interaction
    g_signal_connect(G_OBJECT(window), "key_press_event", G_CALLBACK(keypress), NULL);

    // Vertical widget container
    GtkWidget *box = gtk_vbox_new(FALSE, 0);
    GtkWidget *controls = control_bar(&data);

    gtk_box_pack_start(GTK_BOX(box), controls, FALSE, FALSE, 0);

    // Create a drawing area
    GtkWidget *drawing_area = gtk_drawing_area_new();
    //gtk_widget_set_size_request(drawing_area, 1400, 850);
    // Expose event is our trigger to redraw
    g_signal_connect(G_OBJECT(drawing_area), "expose_event",
            G_CALLBACK(expose_event), (gpointer)&data);

    // Add the drawing area to the box
    gtk_box_pack_start(GTK_BOX(box), drawing_area,TRUE, TRUE, 0);

    // Add the box to the main window
    gtk_container_add(GTK_CONTAINER(window), box);

    // Show the stuff we just created, main window last
    gtk_widget_show(drawing_area);
    gtk_widget_show(box);
    gtk_widget_show(window);
    
    // Main loop
    gtk_main();

    /* cleanup */
    g_list_free(data.steps);
    g_list_free(data.poses);
}

/***********************************************/
int main(int argc, char **argv)
{
    /* pre-test */
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/pack_unpack", pack_unpack_test);
    g_test_run();

    /* UI */
    launch_ui(argc, argv);

    return 0;
}
