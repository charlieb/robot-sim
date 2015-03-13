#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

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
    unsigned int *pos_data;
    enum robot_type type;
};
void to_coords(unsigned int *x, unsigned int *y, float llen, float rlen, enum robot_type type)
{
    float d = 200.0;
    *x = (d*d + llen*llen - rlen*rlen) / (2.0 * d);
    *y = sqrt(*x * *x + llen*llen);
}

void recalc_draw_data(struct draw_data *data)
{
    const float ustep = 1.0 / 4.0;
    gboolean pen_down = FALSE;
    unsigned int x, y;
    float rlen = 150.0f, llen = 150.0f;
    char lrp[3]; // left, right, pen

    g_list_free(data->poses);
    if(data->pos_data != NULL) free(data->pos_data);

    // allocate enough memory for one set of coords for every step.
    data->pos_data = malloc(2 * g_list_length(data->steps) * sizeof(unsigned int));
    memset(data->pos_data, 0, 2 * g_list_length(data->steps) * sizeof(unsigned int));

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
        to_coords(&x, &y, llen, rlen, data->type);
        // only record a new pos if we moved to a new pixel position
        if(data->poses == NULL || 
                ((int*)data->poses->data)[0] != x ||
                ((int*)data->poses->data)[1] != y) {
            int nposes = g_list_length(data->poses);
            data->poses = g_list_prepend(data->poses, &data->pos_data[nposes * 2]);
            data->pos_data[nposes * 2 + 0] = x;
            data->pos_data[nposes * 2 + 1] = y;
            printf("(%i, %i)\n", x,y);
        }
    }
    data->poses = g_list_reverse(data->poses);
}

/******************* UI *************************/

static gboolean expose_event(GtkWidget *widget, GdkEventExpose *event, 
        gpointer gdata)
{
    struct draw_data *data = gdata;

    for(GList *points = data->poses; points != NULL; points = points->next)
        gdk_draw_point(widget->window,
                    widget->style->fg_gc[gtk_widget_get_state (widget)],
                    ((int*)points->data)[0],
                    ((int*)points->data)[1]);
    return TRUE;
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


void launch_ui(int argc, char **argv)
{
    GtkWidget *window;

    gtk_init(&argc, &argv);

    struct draw_data data = {0,};

    data.steps = read_data("input.txt");
    data.type = wires;
    recalc_draw_data(&data);

    // Main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    // Main window close signal handlers
    g_signal_connect(window, "delete-event", G_CALLBACK(delete_event), NULL);
    g_signal_connect(window, "destroy", G_CALLBACK(destroy), NULL);

    // Give us a little border around the drawingarea I'm about to add
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    
    // Create a drawing area
    GtkWidget *drawing_area = gtk_drawing_area_new();
    gtk_widget_set_size_request(drawing_area, 100, 100);
    // Expose event is our trigger to redraw
    g_signal_connect (G_OBJECT(drawing_area), "expose_event",
            G_CALLBACK(expose_event), (gpointer)&data);

    // Add the drawing area to the main window
    gtk_container_add(GTK_CONTAINER(window), drawing_area);

    // Show the stuff we just created, main window last
    gtk_widget_show(drawing_area);
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
