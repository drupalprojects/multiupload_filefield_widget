<?php

/**
 * @file
 * Field module functionality for the Multiple File Widget module.
 */

/**
 * Implements hook_field_widget_info().
 */
function multiupload_filefield_widget_field_widget_info() {
  return array(
    'file_mfw' => array(
      'label' => t('Multiupload'),
      'field types' => array('file'),
      'behaviors' => array(
        'multiple values' => FIELD_BEHAVIOR_CUSTOM,
        'default value' => FIELD_BEHAVIOR_NONE,
      ),
      'settings' => array(
        'progress_indicator' => 'throbber',
      ),
    ),
  );
}

 /**
 * Implements hook_field_widget_form().
 *
 * Mostly copied from drupal core module /modules/file/file.field.inc
 */
function multiupload_filefield_widget_field_widget_form(&$form, &$form_state, $field, $instance, $langcode, $items, $delta, $element) {
  $field_name = $field['field_name'];
  $parents = $form['#parents'];

  // Load the items for form rebuilds from the field state as they might not
  // be in $form_state->getValues() because of validation limitations. Also,
  // they are only passed in as $items when editing existing entities.
  $field_state = field_form_get_state($parents, $field_name, $langcode, $form_state);
  if (isset($field_state['items'])) {
    $items = $field_state['items'];
  }

  // Essentially we use the mfw_managed_file type, extended with some enhancements.
  $element_info = element_info('mfw_managed_file');
  $element += array(
    '#type' => 'mfw_managed_file',
    '#upload_location' => file_field_widget_uri($field, $instance),
    '#upload_validators' => file_field_widget_upload_validators($field, $instance),
    '#value_callback' => 'mfw_field_widget_value',
    '#process' => array_merge($element_info['#process'], array('mfw_field_widget_process')),
    '#progress_indicator' => $instance['widget']['settings']['progress_indicator'],
    // Allows this field to return an array instead of a single value.
    '#extended' => TRUE,
  );

  $defaults = array(
    'fids' => array(),
    'display' => (bool) $field['settings']['display_default'],
    'description' => '',
  );

  // Determine the number of widgets to display.
  $cardinality = $field['cardinality'];
  switch ($cardinality) {
    case FIELD_CARDINALITY_UNLIMITED:
      $max = count($items);
      $is_multiple = TRUE;
      break;

    default:
      $max = $cardinality - 1;
      $is_multiple = ($cardinality > 1);
      break;
  }

  $elements = array();

  $delta = 0;
  // Add an element for every existing item.
  foreach ($items as $item) {
    if ($element) {
      // Input field for the delta (drag-n-drop reordering).
      if ($is_multiple) {
        // We name the element '_weight' to avoid clashing with elements
        // defined by widget.
        $element['_weight'] = array(
          '#type' => 'weight',
          '#title' => t('Weight for row @number', array('@number' => $delta + 1)),
          '#title_display' => 'invisible',
          // Note: this 'delta' is the FAPI #type 'weight' element's property.
          '#delta' => $max,
          '#default_value' => $item->_weight ?: $delta,
          '#weight' => 100,
        );
      }

      $elements[$delta] = $element;
      $delta++;
    }
  }

  $empty_single_allowed = ($cardinality == 1 && $delta == 0);
  $empty_multiple_allowed = ($cardinality == FIELD_CARDINALITY_UNLIMITED || $delta < $cardinality) && empty($form_state['programmed']);

  // Add one more empty row for new uploads except when this is a programmed
  // multiple form as it is not necessary.
  if ($empty_single_allowed || $empty_multiple_allowed) {
    // Create a new empty item.
    $elements[$delta] = $element;
    $elements[$delta]['#default_value'] = $defaults;
    $elements[$delta]['#weight'] = $delta;
    $elements[$delta]['#required'] = ($element['#required'] && $delta == 0);
  }

  if ($is_multiple) {
    // The group of elements all-together need some extra functionality after
    // building up the full list (like draggable table rows).
    $elements['#file_upload_delta'] = $delta;
    $elements['#type'] = 'details';
    $elements['#open'] = TRUE;
    $elements['#theme'] = 'multiupload_filefield_widget_multiple';
    $elements['#theme_wrappers'] = array('fieldset');
    $elements['#process'] = array('mfw_field_widget_process_multiple');
    $elements['#title'] = $element['#title'];
    $elements['#description'] = $element['#description'];
    $elements['#field_name'] = $element['#field_name'];
    $elements['#language'] = $element['#language'];
    $elements['#display_field'] = $field['settings']['display_field'];

    // Add some properties that will eventually be added to the file upload
    // field. These are added here so that they may be referenced easily
    // through a hook_form_alter().
    $elements['#file_upload_title'] = t('Add a new file');
    $elements['#file_upload_description'] = theme('file_upload_help', array('description' => '', 'upload_validators' => $elements[0]['#upload_validators']));
  }

  return $elements;
}

/**
 * Implements hook_field_widget_settings_form().
 */
function multiupload_filefield_widget_field_widget_settings_form($field, $instance) {
  $form = file_field_widget_settings_form($field, $instance) ;
  $form['#attached']['js'] = array(drupal_get_path('module', 'multiupload_filefield_widget') . '/mfw.js');
  return $form;
}

/**
 * Get the upload validators for a file field.
 *
 * @param $field
 *   A field array.
 * @return
 *   An array suitable for passing to file_save_upload() or the file field
 *   element's '#upload_validators' property.
 */

function multiupload_filefield_widget_field_widget_upload_validators($field, $instance) {
  return file_field_widget_upload_validators($field, $instance);
}

/**
 * The #value_callback for the file_mfw field element.
 */
function mfw_field_widget_value($element, $input = FALSE, &$form_state) {
  if ($input) {
    // Checkboxes lose their value when empty.
    // If the display field is present make sure its unchecked value is saved.
    $field = field_widget_field($element, $form_state);
    if (empty($input['display'])) {
      $input['display'] = $field['settings']['display_field'] ? 0 : 1;
    }
  }

  // We depend on the mfw managed file element to handle uploads.
  $return = mfw_managed_file_value($element, $input, $form_state);

  // Ensure that all the required properties are returned even if empty.
  $return += array(
      'fid' => 0,
      'display' => 1,
      'description' => '',
  );

  $last_parent = $element['#parents'][count($element['#parents']) - 1];
  $form_state['values'][$element['#field_name']]['und'][$last_parent] = $return;

  return $return;
}

/**
 * An element #process callback for the mfw_file field type.
 *
 * Expands the mfw_file type to include the description and display fields.
 */
function mfw_field_widget_process($element, &$form_state, &$form) {
  $item = $element['#value'];
  $item['fids'] = $element['fids']['#value'];

  $field = field_widget_field($element, $form_state);
  $instance = field_widget_instance($element, $form_state);
  $settings = $instance['widget']['settings'];

  $element['#theme'] = 'multiupload_filefield_widget';

  // Add the display field if enabled.
  if (!empty($element['#display_field'])) {
    $element['display'] = array(
      '#type' => empty($item['fids']) ? 'hidden' : 'checkbox',
      '#title' => t('Include file in display'),
      '#attributes' => array('class' => array('file-display')),
    );
    if (isset($item['display'])) {
      $element['display']['#value'] = $item['display'] ? '1' : '';
    } else {
      $element['display']['#value'] = $element['#display_default'];
    }
  }
  else {
    $element['display'] = array(
      '#type' => 'hidden',
      '#value' => '1',
    );
  }

  // Add the description field if enabled.
  if (!empty($element['#description_field']) && $item['fids']) {
    $element['description'] = array(
      '#type' => variable_get('file_description_type', 'textfield'),
      '#title' => t('Description'),
      '#value' => isset($item['description']) ? $item['description'] : '',
      '#maxlength' => variable_get('file_description_length', 128),
      '#description' => t('The description may be used as the label of the link to the file.'),
    );
  }

  // Adjust the Ajax settings so that on upload and remove of any individual
  // file, the entire group of file fields is updated together.
  if ($field['cardinality'] != 1) {
    $parents = array_slice($element['#array_parents'], 0, -1);
    $new_path = 'file/ajax/' . implode('/', $parents) . '/' . $form['form_build_id']['#value'];
    $field_element = drupal_array_get_nested_value($form, $parents);
    $new_wrapper = $field_element['#id'] . '-ajax-wrapper';
    foreach (element_children($element) as $key) {
      if (isset($element[$key]['#ajax'])) {
        $element[$key]['#ajax']['path'] = $new_path;
        $element[$key]['#ajax']['wrapper'] = $new_wrapper;
      }
    }
    unset($element['#prefix'], $element['#suffix']);
  }

  // Add another submit handler to the upload and remove buttons, to implement
  // functionality needed by the field widget. This submit handler, along with
  // the rebuild logic in file_field_widget_form() requires the entire field,
  // not just the individual item, to be valid.
  foreach (array('upload_button', 'remove_button') as $key) {
    $element[$key]['#submit'][] = 'file_field_widget_submit';
    $element[$key]['#limit_validation_errors'] = array(array_slice($element['#parents'], 0, -1));
  }
  return $element;
}

/**
 * An element #process callback for a group of mfw_file fields.
 *
 * Mostly copied from drupal core module /module/file/file.field.inc.
 *
 * Adds the weight field to each row so it can be ordered and adds a new AJAX
 * wrapper around the entire group so it can be replaced all at once.
 */
function mfw_field_widget_process_multiple($element, &$form_state, $form) {
  $element_children = element_children($element);
  $count = count($element_children);

  // Count the number of already uploaded files, in order to display new
  // items in \Drupal\file\Element\ManagedFile::uploadAjaxCallback().
  if (empty($form_state['rebuild'])) {
    $count_items_before = 0;
    foreach ($element_children as $children) {
      if (!empty($element[$children]['#default_value']['fids'])) {
        $count_items_before++;
      }
    }

    $form_state['file_upload_delta_initial'] = $count_items_before;
  }

  foreach ($element_children as $delta => $key) {
    if ($key != $element['#file_upload_delta']) {
      $description = _file_field_get_description_from_element($element[$key]);
      $element[$key]['_weight'] = array(
        '#type' => 'weight',
        '#title' => $description ? t('Weight for @title', array('@title' => $description)) : t('Weight for new file'),
        '#title_display' => 'invisible',
        '#delta' => $count,
        '#default_value' => $delta,
      );
    }
    else {
      // The title needs to be assigned to the upload field so that validation
      // errors include the correct widget label.
      $element[$key]['#title'] = $element['#title'];
      $element[$key]['_weight'] = array(
        '#type' => 'hidden',
        '#default_value' => $delta,
      );
    }
  }

  // Add a new wrapper around all the elements for Ajax replacement.
  $element['#prefix'] = '<div id="' . $element['#id'] . '-ajax-wrapper">';
  $element['#suffix'] = '</div>';

  return $element;
}

/**
 * Submit handler for upload and remove buttons of file_mfw fields.
 *
 * This runs in addition to and after file_managed_file_submit().
 *
 * @see file_managed_file_submit()
 * @see file_field_widget_form()
 * @see file_field_widget_process()
 */
function multiupload_filefield_widget_field_widget_submit($form, &$form_state) {
  // During the form rebuild, formElement() will create field item widget
  // elements using re-indexed deltas, so clear out $form_state['input'] to
  // avoid a mismatch between old and new deltas. The rebuilt elements will
  // have #default_value set appropriately for the current state of the field,
  // so nothing is lost in doing this.
  $button = $form_state['triggering_element'];
  $parents = array_slice($button['#parents'], 0, -2);
  drupal_array_set_nested_value($form_state['input'], $parents, NULL);

  // Go one level up in the form, to the widgets container.
  $element = drupal_array_get_nested_value($form, array_slice($button['#array_parents'], 0, -1));
  $field_name = $element['#field_name'];
  $langcode = $element['#language'];
  $parents = $element['#field_parents'];

  $submitted_values = drupal_array_get_nested_value($form_state['values'], array_slice($button['#parents'], 0, -2));
  foreach ($submitted_values as $delta => $submitted_value) {
    if (empty($submitted_value['fids'])) {
      unset($submitted_values[$delta]);
    }
  }

  // If there are more files uploaded via the same widget, we have to separate
  // them, as we display each file in it's own widget.
  $new_values = array();
  foreach ($submitted_values as $delta => $submitted_value) {
    if (is_array($submitted_value['fids'])) {
      foreach ($submitted_value['fids'] as $fid) {
        $new_value = $submitted_value;
        $new_value['fids'] = array($fid);
        $new_values[] = $new_value;
      }
    }
    else {
      $new_value = $submitted_value;
    }
  }

  // Re-index deltas after removing empty items.
  $submitted_values = array_values($new_values);

  // Update form_state values.
  drupal_array_set_nested_value($form_state['values'], array_slice($button['#parents'], 0, -2), $submitted_values);

  // Update items.
  $field_state = field_form_get_state($parents, $field_name, $langcode, $form_state);
  $field_state['items'] = $submitted_values;
  field_form_set_state($parents, $field_name, $langcode, $form_state, $field_state);
}
