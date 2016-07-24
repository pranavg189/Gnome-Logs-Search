#include "gl-search-popover.h"
#include "gl-enums.h"

#include <glib/gi18n.h>

struct _GlSearchPopover
{
    /*< private >*/
    GtkPopover parent_instance;
};

typedef struct
{
    /* Search popover elements */
    GtkWidget *parameter_stack;
    GtkWidget *parameter_button_label;
    GtkWidget *parameter_label_stack;
    GtkWidget *parameter_treeview;
    GtkListStore *parameter_liststore;
    GtkWidget *search_type_revealer;
    GtkWidget *range_stack;
    GtkWidget *range_label_stack;
    GtkWidget *range_treeview;
    GtkWidget *range_button_label;
    GtkWidget *clear_range_button;
    GtkWidget *range_button_drop_down_image;
    GtkListStore *range_liststore;

    gulong parameter_group;
    GlQuerySearchType search_type;
    gsize range_group;
} GlSearchPopoverPrivate;

enum
{
    PARAMETER_GROUP,
    RANGE_GROUP,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_SEARCH_TYPE,
    N_PROPERTIES
};

enum
{
    COLUMN_PARAMETER_NAME,
    COLUMN_PARAMETER_INDEX,
    COLUMN_SHOW_SEPARATOR,
    N_COLUMNS
};

static guint signals[LAST_SIGNAL];
static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GlSearchPopover, gl_search_popover, GTK_TYPE_POPOVER)

/* Event handlers for search popover elements */
static void
search_popover_closed (GtkPopover *popover,
                       gpointer user_data)
{
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");
}

static void
select_parameter_button_clicked (GtkButton *button,
                                 gpointer user_data)
{
    GlSearchPopoverPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    gchar *path_string;

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "select-parameter-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");

    path_string = g_strdup_printf ("%lu:0", priv->parameter_group);

    path = gtk_tree_path_new_from_string (path_string);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->parameter_treeview));

    gtk_tree_selection_select_path (selection, path);

    gtk_tree_path_free (path);
    g_free (path_string);
}

static gboolean
parameter_treeview_row_seperator (GtkTreeModel *model,
                                  GtkTreeIter *iter,
                                  gpointer user_data)
{
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    gboolean show_separator;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), iter,
                        COLUMN_SHOW_SEPARATOR, &show_separator,
                        -1);

    return show_separator;
}

static void
on_parameter_treeview_row_activated (GtkTreeView *tree_view,
                                     GtkTreePath *path,
                                     GtkTreeViewColumn *column,
                                     gpointer user_data)
{
    GtkTreeIter iter;
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    gchar *parameter_label;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->parameter_liststore), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->parameter_liststore), &iter,
                        COLUMN_PARAMETER_NAME, &parameter_label,
                        COLUMN_PARAMETER_INDEX, &priv->parameter_group,
                        -1);

    gtk_label_set_label (GTK_LABEL (priv->parameter_button_label),
                         _(parameter_label));

    /* Do not Show "Search Type" option if all available fields group is selected */
    if (priv->parameter_group == GL_PARAMETER_GROUP_ALL_AVAILABLE_FIELDS)
    {
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->search_type_revealer), FALSE);
        gtk_widget_set_visible (priv->search_type_revealer, FALSE);
    }
    else
    {
        gtk_widget_set_visible (priv->search_type_revealer, TRUE);
        gtk_revealer_set_reveal_child (GTK_REVEALER (priv->search_type_revealer), TRUE);
    }

    /* Inform GlEventViewlist about parameter change */
    g_signal_emit_by_name (popover, "parameter-group", priv->parameter_group);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    g_free (parameter_label);
}

static void
search_type_changed (GtkToggleButton *togglebutton,
                     gpointer user_data)
{
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    GEnumClass *eclass;
    GEnumValue *evalue;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);
    eclass = g_type_class_ref (GL_TYPE_QUERY_SEARCH_TYPE);

    if (gtk_toggle_button_get_active (togglebutton))
    {
        evalue = g_enum_get_value (eclass, SEARCH_TYPE_EXACT);
    }
    else
    {
        evalue = g_enum_get_value (eclass, SEARCH_TYPE_SUBSTRING);
    }

    priv->search_type = evalue->value;

    /* Inform GlEventViewlist about search type property change */
    g_object_notify_by_pspec (G_OBJECT (popover),
                              obj_properties[PROP_SEARCH_TYPE]);

    g_type_class_unref (eclass);
}

GlQuerySearchType
gl_search_popover_get_query_search_type (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (popover);

    return priv->search_type;
}

GType
gl_query_search_type_get_type (void)
{
    static GType type = 0;

    if (!type)
    {
        static const GEnumValue _gl_query_search_type_values[] = {
            { SEARCH_TYPE_EXACT, "SEARCH_TYPE_EXACT", "exact" },
            { SEARCH_TYPE_SUBSTRING, "SEARCH_TYPE_SUBSTRING", "substring" },
            { 0, NULL, NULL }
        };

        type = g_enum_register_static ("GlQuerySearchType", _gl_query_search_type_values);
    }

  return type;
}

static void
gl_search_popover_get_property (GObject *object,
                                guint prop_id,
                                GValue *value,
                                GParamSpec *pspec)
{
    GlSearchPopover *popover = GL_SEARCH_POPOVER (object);
    GlSearchPopoverPrivate *priv = gl_search_popover_get_instance_private (popover);

    switch (prop_id)
    {
        case PROP_SEARCH_TYPE:
            g_value_set_enum (value, priv->search_type);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
gl_search_popover_set_property (GObject *object,
                                guint prop_id,
                                const GValue *value,
                                GParamSpec *pspec)
{
    GlSearchPopover *popover = GL_SEARCH_POPOVER (object);
    GlSearchPopoverPrivate *priv = gl_search_popover_get_instance_private (popover);

    switch (prop_id)
    {
        case PROP_SEARCH_TYPE:
            priv->search_type = g_value_get_enum (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
select_range_button_clicked (GtkButton *button,
                             gpointer user_data)
{
    GlSearchPopoverPrivate *priv;
    GtkTreeSelection *selection;
    GtkTreePath *path;
    gchar *path_string;
    gsize range_group_row;

    priv = gl_search_popover_get_instance_private (GL_SEARCH_POPOVER (user_data));

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-list");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "show-log-from-label");

    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_stack), "parameter-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->parameter_label_stack), "what-label");

    /* Select the row in treeview which was shown in the parameter label */
    range_group_row = priv->range_group;

    path_string = g_strdup_printf ("%" G_GSIZE_FORMAT ":0", range_group_row);

    path = gtk_tree_path_new_from_string (path_string);

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (priv->range_treeview));

    gtk_tree_selection_select_path (selection, path);

    gtk_tree_path_free (path);
    g_free (path_string);
}

static gboolean
range_treeview_row_seperator (GtkTreeModel *model,
                              GtkTreeIter *iter,
                              gpointer user_data)
{
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;
    gboolean show_seperator;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->range_liststore), iter,
                        2, &show_seperator,
                        -1);

    return show_seperator;
}

static void
on_range_treeview_row_activated (GtkTreeView *tree_view,
                                 GtkTreePath *path,
                                 GtkTreeViewColumn *column,
                                 gpointer user_data)
{
    GtkTreeIter iter;
    gchar *range_label;
    GlSearchPopover *popover;
    GlSearchPopoverPrivate *priv;

    popover = GL_SEARCH_POPOVER (user_data);

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_model_get_iter (GTK_TREE_MODEL (priv->range_liststore), &iter, path);

    gtk_tree_model_get (GTK_TREE_MODEL (priv->range_liststore), &iter,
                        0, &range_label,
                        1, &priv->range_group,
                        -1);

    gtk_label_set_label (GTK_LABEL (priv->range_button_label),
                         _(range_label));

    /* Show "Clear Range" Button if other than "Current Boot" is selected */
    if(priv->range_group == GL_RANGE_GROUP_CURRENT_BOOT)
    {
        gtk_widget_hide (priv->clear_range_button);
        gtk_widget_show (priv->range_button_drop_down_image);
    }
    else
    {
        gtk_widget_show (priv->clear_range_button);
        gtk_widget_hide (priv->range_button_drop_down_image);
    }

    g_signal_emit_by_name (popover, "range-group", priv->range_group);

    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_stack), "range-button");
    gtk_stack_set_visible_child_name (GTK_STACK (priv->range_label_stack), "when-label");

    g_free (range_label);
}

static void
clear_range_button_clicked (GtkButton *button,
                            gpointer user_data)
{
    GlSearchPopover *popover = GL_SEARCH_POPOVER (user_data);
    GlSearchPopoverPrivate *priv;

    priv = gl_search_popover_get_instance_private (popover);

    gtk_widget_hide (priv->clear_range_button);
    gtk_widget_show (priv->range_button_drop_down_image);

    gtk_label_set_label (GTK_LABEL (priv->range_button_label),
                           _("Current Boot"));

    priv->range_group = GL_RANGE_GROUP_CURRENT_BOOT;

    g_signal_emit_by_name (popover, "range-group", priv->range_group);
}

static void
gl_search_popover_class_init (GlSearchPopoverClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    gobject_class->get_property = gl_search_popover_get_property;
    gobject_class->set_property = gl_search_popover_set_property;

    obj_properties[PROP_SEARCH_TYPE] = g_param_spec_enum ("search-type", "Search Type",
                                                          "Do exact or substring search",
                                                          GL_TYPE_QUERY_SEARCH_TYPE,
                                                          SEARCH_TYPE_SUBSTRING,
                                                          G_PARAM_READWRITE |
                                                          G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties (gobject_class, N_PROPERTIES,
                                       obj_properties);

    signals[PARAMETER_GROUP] = g_signal_new ("parameter-group",
                                             GL_TYPE_SEARCH_POPOVER,
                                             G_SIGNAL_RUN_LAST,
                                             0,
                                             NULL,
                                             NULL,
                                             g_cclosure_marshal_generic,
                                             G_TYPE_NONE,
                                             1,
                                             G_TYPE_ULONG);

    signals[RANGE_GROUP] = g_signal_new ("range-group",
                                         GL_TYPE_SEARCH_POPOVER,
                                         G_SIGNAL_RUN_LAST,
                                         0,
                                         NULL,
                                         NULL,
                                         g_cclosure_marshal_generic,
                                         G_TYPE_NONE,
                                         1,
                                         G_TYPE_INT);

    gtk_widget_class_set_template_from_resource (widget_class,
                                                 "/org/gnome/Logs/gl-searchpopover.ui");
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_button_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_label_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_treeview);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  parameter_liststore);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  search_type_revealer);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_label_stack);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_treeview);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_button_label);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  clear_range_button);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_button_drop_down_image);
    gtk_widget_class_bind_template_child_private (widget_class, GlSearchPopover,
                                                  range_liststore);


    gtk_widget_class_bind_template_callback (widget_class,
                                             search_popover_closed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             select_parameter_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_parameter_treeview_row_activated);
    gtk_widget_class_bind_template_callback (widget_class,
                                             search_type_changed);
    gtk_widget_class_bind_template_callback (widget_class,
                                             select_range_button_clicked);
    gtk_widget_class_bind_template_callback (widget_class,
                                             on_range_treeview_row_activated);
    gtk_widget_class_bind_template_callback (widget_class,
                                             clear_range_button_clicked);
}

static void
gl_search_popover_init (GlSearchPopover *popover)
{
    GlSearchPopoverPrivate *priv;

    gtk_widget_init_template (GTK_WIDGET (popover));

    priv = gl_search_popover_get_instance_private (popover);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->parameter_treeview),
                                          (GtkTreeViewRowSeparatorFunc) parameter_treeview_row_seperator,
                                          popover,
                                          NULL);

    gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (priv->range_treeview),
                                          (GtkTreeViewRowSeparatorFunc) range_treeview_row_seperator,
                                          popover,
                                          NULL);

    /* Set "All Available Fields" as default option in the select parameter button */
    priv->parameter_group = GL_PARAMETER_GROUP_ALL_AVAILABLE_FIELDS;

    /* Set "Current Boot" as the default journal range */
    priv->range_group = GL_RANGE_GROUP_CURRENT_BOOT;
}

GtkWidget *
gl_search_popover_new (void)
{
    return g_object_new (GL_TYPE_SEARCH_POPOVER, NULL);
}
