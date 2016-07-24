#ifndef GL_SEARCH_POPOVER_H_
#define GL_SEARCH_POPOVER_H_

#include <gtk/gtk.h>
#include "gl-journal-model.h"

G_BEGIN_DECLS

/* Rows in parameter treeview */
typedef enum
{
    GL_PARAMETER_GROUP_ALL_AVAILABLE_FIELDS,
    GL_PARAMETER_GROUP_PID = 2, /* Row 1 is a separator in parameter liststore */
    GL_PARAMETER_GROUP_UID,
    GL_PARAMETER_GROUP_GID,
    GL_PARAMETER_GROUP_MESSAGE,
    GL_PARAMETER_GROUP_PROCESS_NAME,
    GL_PARAMETER_GROUP_SYSTEMD_UNIT,
    GL_PARAMETER_GROUP_KERNEL_DEVICE,
    GL_PARAMETER_GROUP_AUDIT_SESSION,
    GL_PARAMETER_GROUP_EXECUTABLE_PATH
} GlParameterGroups;

typedef enum
{
    GL_RANGE_GROUP_CURRENT_BOOT,
    GL_RANGE_GROUP_PREVIOUS_BOOT,
    GL_RANGE_GROUP_TODAY = 3,
    GL_RANGE_GROUP_YESTERDAY = 4,
    GL_RANGE_GROUP_LAST_3_DAYS = 5,
    GL_RANGE_GROUP_ENTIRE_JOURNAL = 7
} GlRangeGroups;

#define GL_TYPE_SEARCH_POPOVER (gl_search_popover_get_type ())
G_DECLARE_FINAL_TYPE (GlSearchPopover, gl_search_popover, GL, SEARCH_POPOVER, GtkPopover)

GtkWidget * gl_search_popover_new (void);
GlQuerySearchType gl_search_popover_get_query_search_type (GlSearchPopover *popover);

GType gl_query_search_type_get_type (void);
#define GL_TYPE_QUERY_SEARCH_TYPE (gl_query_search_type_get_type())

G_END_DECLS

#endif /* GL_SEARCH_POPOVER_H_ */
