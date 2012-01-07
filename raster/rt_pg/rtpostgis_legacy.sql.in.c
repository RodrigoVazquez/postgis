-------------------------------------------------------------------------------
--
-- $Id$
--
-- PostGIS Raster - Raster Type for PostGIS
-- http://trac.osgeo.org/postgis/wiki/WKTRaster
--
-- Copyright (C) 2012 Regents of the University of California
--   <bkpark@ucdavis.edu>
--
-- This is free software; you can redistribute and/or modify it under
-- the terms of the GNU General Public Licence. See the COPYING file.
--
-------------------------------------------------------------------------------

--
-- For legacy access to OLD versions of raster_columns AND raster_overviews
--

-- raster_columns and raster_overviews tables no longer exist
-- if tables found, rename tables
DROP FUNCTION IF EXISTS _rename_raster_tables();
CREATE OR REPLACE FUNCTION _rename_raster_tables()
	RETURNS void AS $$
	DECLARE
		cnt int;
	BEGIN
		SELECT count(*) INTO cnt
		FROM pg_class c
		JOIN pg_namespace n
			ON c.relnamespace = n.oid
		WHERE c.relname = 'raster_columns'
			AND c.relkind = 'r'::char
			AND NOT pg_is_other_temp_schema(c.relnamespace);

		IF cnt > 0 THEN
			EXECUTE 'ALTER TABLE raster_columns RENAME TO deprecated_raster_columns';
		END IF;

		SELECT count(*) INTO cnt
		FROM pg_class c
		JOIN pg_namespace n
			ON c.relnamespace = n.oid
		WHERE c.relname = 'raster_overviews'
			AND c.relkind = 'r'::char
			AND NOT pg_is_other_temp_schema(c.relnamespace);

		IF cnt > 0 THEN
			EXECUTE 'ALTER TABLE raster_overviews RENAME TO deprecated_raster_overviews';
		END IF;

	END;
	$$ LANGUAGE 'plpgsql' VOLATILE;
SELECT _rename_raster_tables();
DROP FUNCTION _rename_raster_tables();

CREATE OR REPLACE VIEW raster_columns AS
	SELECT
		current_database() AS r_table_catalog,
		n.nspname AS r_table_schema,
		c.relname AS r_table_name,
		a.attname AS r_raster_column,
		COALESCE(_raster_constraint_info_srid(n.nspname, c.relname, a.attname), (SELECT ST_SRID('POINT(0 0)'::geometry))) AS srid,
		_raster_constraint_info_scale(n.nspname, c.relname, a.attname, 'x') AS scale_x,
		_raster_constraint_info_scale(n.nspname, c.relname, a.attname, 'y') AS scale_y,
		_raster_constraint_info_blocksize(n.nspname, c.relname, a.attname, 'width') AS blocksize_x,
		_raster_constraint_info_blocksize(n.nspname, c.relname, a.attname, 'height') AS blocksize_y,
		COALESCE(_raster_constraint_info_alignment(n.nspname, c.relname, a.attname), FALSE) AS same_alignment,
		COALESCE(_raster_constraint_info_regular_blocking(n.nspname, c.relname, a.attname), FALSE) AS regular_blocking,
		_raster_constraint_info_num_bands(n.nspname, c.relname, a.attname) AS num_bands,
		_raster_constraint_info_pixel_types(n.nspname, c.relname, a.attname) AS pixel_types,
		_raster_constraint_info_nodata_values(n.nspname, c.relname, a.attname) AS nodata_values,
		_raster_constraint_info_extent(n.nspname, c.relname, a.attname) AS extent,
		a.attname AS r_column,
		FALSE AS out_db
	FROM
		pg_class c,
		pg_attribute a,
		pg_type t,
		pg_namespace n
	WHERE t.typname = 'raster'::name
		AND a.attisdropped = false
		AND a.atttypid = t.oid
		AND a.attrelid = c.oid
		AND c.relnamespace = n.oid
		AND (c.relkind = 'r'::"char" OR c.relkind = 'v'::"char")
		AND NOT pg_is_other_temp_schema(c.relnamespace);

CREATE OR REPLACE VIEW raster_overviews AS
	SELECT
		current_database() AS o_table_catalog,
		n.nspname AS o_table_schema,
		c.relname AS o_table_name,
		a.attname AS o_raster_column,
		current_database() AS r_table_catalog,
		split_part(split_part(s.consrc, '''::name', 1), '''', 2)::name AS r_table_schema,
		split_part(split_part(s.consrc, '''::name', 2), '''', 2)::name AS r_table_name,
		split_part(split_part(s.consrc, '''::name', 3), '''', 2)::name AS r_raster_column,
		trim(both from split_part(s.consrc, ',', 2))::integer AS overview_factor,
		a.attname AS o_column,
		split_part(split_part(s.consrc, '''::name', 3), '''', 2)::name AS r_column,
		FALSE AS out_db
	FROM
		pg_class c,
		pg_attribute a,
		pg_type t,
		pg_namespace n,
		pg_constraint s
	WHERE t.typname = 'raster'::name
		AND a.attisdropped = false
		AND a.atttypid = t.oid
		AND a.attrelid = c.oid
		AND c.relnamespace = n.oid
		AND (c.relkind = 'r'::"char" OR c.relkind = 'v'::"char")
		AND s.connamespace = n.oid
		AND s.conrelid = c.oid
		AND s.consrc LIKE '%_overview_constraint(%'
		AND NOT pg_is_other_temp_schema(c.relnamespace);