#include <gtk/gtk.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    gchar *text;
    gboolean done;
    gchar *category;
    gchar *priority;
    time_t due_date;
} Task;

GtkListStore *store;
GtkWidget *entry, *category_combo, *priority_combo;
GtkWidget *progress_bar;

// Convert time_t to YYYY-MM-DD string
void format_date(time_t t, char *buf) {
    struct tm *tm_info = localtime(&t);
    strftime(buf, 11, "%Y-%m-%d", tm_info);
}

// Add new task
void add_task(GtkWidget *widget, gpointer treeview) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
    if (text[0] == '\0') return;

    GtkTreeIter iter;
    gtk_list_store_append(store, &iter);

    const gchar *category = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(category_combo));
    const gchar *priority = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(priority_combo));
    time_t due = time(NULL) + 86400 * 7; // default due in 7 days

    gtk_list_store_set(store, &iter,
                       0, text,
                       1, FALSE,
                       2, category,
                       3, priority,
                       4, due,
                       -1);
    gtk_entry_set_text(GTK_ENTRY(entry), "");
    g_free((gpointer)category);
    g_free((gpointer)priority);

    gtk_widget_grab_focus(entry);
}

// Toggle task done
void toggle_done(GtkCellRendererToggle *cell, gchar *path_str, gpointer treeview) {
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    GtkTreeIter iter;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);

    gboolean done;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 1, &done, -1);
    gtk_list_store_set(store, &iter, 1, !done, -1);
    gtk_tree_path_free(path);
}

// Update progress bar
void update_progress() {
    GtkTreeIter iter;
    gboolean valid;
    int total = 0, done_count = 0;

    valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gboolean done;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 1, &done, -1);
        if (done) done_count++;
        total++;
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
    gdouble fraction = total ? ((gdouble)done_count / total) : 0;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(progress_bar), fraction);
}

// Row coloring logic
void set_row_colors(GtkTreeViewColumn *col, GtkCellRenderer *renderer, GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
    gchar *priority;
    gboolean done;
    time_t due;
    gtk_tree_model_get(model, iter, 1, &done, 3, &priority, 4, &due, -1);

    GdkRGBA color;
    if (done) gdk_rgba_parse(&color, "gray");
    else if (priority && strcmp(priority, "High") == 0) gdk_rgba_parse(&color, "red");
    else if (priority && strcmp(priority, "Medium") == 0) gdk_rgba_parse(&color, "orange");
    else gdk_rgba_parse(&color, "black");

    time_t now = time(NULL);
    if (!done && due < now) gdk_rgba_parse(&color, "purple");

    g_object_set(renderer, "foreground-rgba", &color, NULL);
    g_free(priority);
}

// Save tasks to file
void save_tasks() {
    FILE *fp = fopen("tasks.csv", "w");
    if (!fp) return;

    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter);
    while (valid) {
        gchar *text, *category, *priority;
        gboolean done;
        time_t due;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                           0, &text, 1, &done, 2, &category, 3, &priority, 4, &due, -1);

        fprintf(fp, "\"%s\",%d,%s,%s,%ld\n", text, done, category, priority, due);

        g_free(text);
        g_free(category);
        g_free(priority);

        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter);
    }
    fclose(fp);
}

// loads task from a csv file
void load_tasks() {
    FILE *fp = fopen("tasks.csv", "r");
    if (!fp) return;

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        char text[256], category[64], priority[64];
        int done;
        long due;
        if (sscanf(line, "\"%[^\"]\",%d,%[^,],%[^,],%ld",
                   text, &done, category, priority, &due) == 5) {

            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                               0, text, 1, done,
                               2, category, 3, priority,
                               4, (time_t)due, -1);
        }
    }
    fclose(fp);
}

// deletes a task
void delete_task(GtkWidget *widget, gpointer treeview) {
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    GtkTreeIter iter;

    if (gtk_tree_selection_get_selected(selection, NULL, &iter)) {
        gtk_list_store_remove(store, &iter);
        update_progress();
    }
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "GUI To-Do List 💪");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);


    // task entry
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter new task");
    gtk_box_pack_start(GTK_BOX(vbox), entry, FALSE, FALSE, 5);

    // dropdown for priority
    category_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(category_combo), "Personal");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(category_combo), "Work");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(category_combo), "Study");
    gtk_combo_box_set_active(GTK_COMBO_BOX(category_combo), 0);
    gtk_box_pack_start(GTK_BOX(vbox), category_combo, FALSE, FALSE, 5);

    // dropdown for priority selection
    priority_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(priority_combo), "High");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(priority_combo), "Medium");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(priority_combo), "Low");
    gtk_combo_box_set_active(GTK_COMBO_BOX(priority_combo), 1);
    gtk_box_pack_start(GTK_BOX(vbox), priority_combo, FALSE, FALSE, 5);

    // add and del buttons
    GtkWidget *add_button = gtk_button_new_with_label("Add Task");
    gtk_box_pack_start(GTK_BOX(vbox), add_button, FALSE, FALSE, 5);

    GtkWidget *delete_button = gtk_button_new_with_label("Delete Task");
    gtk_box_pack_start(GTK_BOX(vbox), delete_button, FALSE, FALSE, 5);

    // stroes the list
    store = gtk_list_store_new(5, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT64);

    GtkWidget *treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

    // this is to add columns in the section
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *col = gtk_tree_view_column_new_with_attributes("Task", renderer, "text", 0, NULL);
    gtk_tree_view_column_set_cell_data_func(col, renderer, set_row_colors, NULL, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(renderer, "toggled", G_CALLBACK(toggle_done), treeview);
    col = gtk_tree_view_column_new_with_attributes("Done", renderer, "active", 1, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Category", renderer, "text", 2, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Priority", renderer, "text", 3, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), col);

    gtk_box_pack_start(GTK_BOX(vbox), treeview, TRUE, TRUE, 5);

    // Progress bar
    progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox), progress_bar, FALSE, FALSE, 5);

    // Signals
    g_signal_connect(add_button, "clicked", G_CALLBACK(add_task), treeview);
    g_signal_connect(delete_button, "clicked", G_CALLBACK(delete_task), treeview);

    load_tasks();
    gtk_widget_show_all(window);

    gtk_main();
    save_tasks();

    return 0;
}